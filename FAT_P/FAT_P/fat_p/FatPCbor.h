/**
 * @file FatPCbor.h
 * @brief FAT-P CBOR serialization adapters
 *
 * @layer Domain
 */
#pragma once

/*
FATP_META:
  meta_version: 1
  component: FatPCbor
  file_role: public_header
  path: fat_p/FatPCbor.h
  namespace: fat_p
  layer: Domain
  summary: "Public header for FatPCbor."
  api_stability: in_work
  related:
    docs_search: "FatPCbor"
    tests:
      - tests/test_FatPCbor.cpp
  hygiene:
    pragma_once: true
    include_guard: false
    defines_total: 1
    defines_unprefixed: 1
    undefs_total: 0
    includes_windows_h: false
  generated:
    by: fatp-meta-tool
    mode: autogen
*/
#include "CborLite.h"
#include "enforce.h"
#include "Expected.h"
#include "HpcVector.h"

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
namespace cbor_fatp
{

// ============================================================================
// Error / Result Types
// ============================================================================

struct CborError
{
    std::string message;

    CborError() = default;

    explicit CborError(std::string msg)
        : message(std::move(msg))
    {
    }
};

template <typename T>
using CborResult = Expected<T, CborError>;

// ============================================================================
// Internal Traits
// ============================================================================

namespace detail
{

template <typename Container, typename = void>
struct is_byte_container : std::false_type
{
};

template <typename Container>
struct is_byte_container<Container,
                         std::void_t<typename Container::value_type,
                                     decltype(std::declval<Container&>().data()),
                                     decltype(std::declval<const Container&>().data()),
                                     decltype(std::declval<Container&>().size()),
                                     decltype(std::declval<Container&>().push_back(std::declval<std::uint8_t>()))>>
    : std::bool_constant<std::is_same_v<typename Container::value_type, std::uint8_t> &&
                         std::is_same_v<decltype(std::declval<Container&>().data()), std::uint8_t*> &&
                         std::is_same_v<decltype(std::declval<const Container&>().data()), const std::uint8_t*> &&
                         std::is_same_v<decltype(std::declval<Container&>().size()), std::size_t>>
{
};

template <typename Container>
inline constexpr bool is_byte_container_v = is_byte_container<Container>::value;

} // namespace detail

// Default high-performance CBOR buffer: HpcVector<uint8_t, Alignment, Policy>
template <typename T = std::uint8_t, std::size_t Alignment = 64, typename Policy = memory::NumaLocalPolicy>
using DefaultCborBuffer = HpcVector<T, Alignment, Policy>;

// Canonical Fat-P CBOR buffer alias
using CborBuffer = DefaultCborBuffer<>;

// ============================================================================
// CborWriter: Policy-free CBOR writer over arbitrary byte containers
// ============================================================================

template <typename Buffer = CborBuffer>
class CborWriter
{
public:
    static_assert(detail::is_byte_container_v<Buffer>,
                  "Buffer must store uint8_t and provide data()/size()/push_back()");

    explicit CborWriter(Buffer& buffer) noexcept
        : mBuffer(buffer)
    {
    }

    CborWriter(const CborWriter&) = delete;
    CborWriter& operator=(const CborWriter&) = delete;

    template <typename UInt, typename = std::enable_if_t<std::is_unsigned_v<UInt> && std::is_integral_v<UInt>>>
    void write_uint(UInt value)
    {
        write_major_type(0U, static_cast<std::uint64_t>(value));
    }

    template <typename SInt, typename = std::enable_if_t<std::is_signed_v<SInt> && std::is_integral_v<SInt>>>
    void write_int(SInt value)
    {
        if (value >= 0)
        {
            write_major_type(0U, static_cast<std::uint64_t>(value));
        }
        else
        {
            const auto encoded = static_cast<std::uint64_t>(-value - 1);
            write_major_type(1U, encoded);
        }
    }

    void write_bool(bool value)
    {
        mBuffer.push_back(static_cast<std::uint8_t>(0xF4U + (value ? 1U : 0U)));
    }

    void write_null()
    {
        mBuffer.push_back(0xF6U);
    }

    void write_double(double value)
    {
        const std::uint8_t header = static_cast<std::uint8_t>((7U << 5U) | 27U);
        mBuffer.push_back(header);

        std::uint64_t bits = 0;
        static_assert(sizeof(bits) == sizeof(value), "Unexpected double size");
        std::memcpy(&bits, &value, sizeof(bits));

        for (int i = 7; i >= 0; --i)
        {
            mBuffer.push_back(static_cast<std::uint8_t>((bits >> (8 * i)) & 0xFFU));
        }
    }

    void write_string(const std::string& value)
    {
        write_major_type(3U, static_cast<std::uint64_t>(value.size()));
        mBuffer.insert(mBuffer.end(),
                       reinterpret_cast<const std::uint8_t*>(value.data()),
                       reinterpret_cast<const std::uint8_t*>(value.data()) + value.size());
    }

    template <typename BytesContainer>
    void write_bytes(const BytesContainer& bytes)
    {
        static_assert(detail::is_byte_container_v<BytesContainer>,
                      "BytesContainer must store uint8_t and provide data()/size()");
        write_major_type(2U, static_cast<std::uint64_t>(bytes.size()));
        mBuffer.insert(mBuffer.end(), bytes.data(), bytes.data() + bytes.size());
    }

    void write_array_header(std::uint64_t size)
    {
        write_major_type(4U, size);
    }

    void write_map_header(std::uint64_t size)
    {
        write_major_type(5U, size);
    }

    std::size_t size() const noexcept
    {
        return mBuffer.size();
    }

private:
    Buffer& mBuffer;

    void write_major_type(std::uint8_t major_type, std::uint64_t arg)
    {
        FATP_ENFORCE(major_type <= 7U, "Invalid CBOR major type");

        if (arg < 24U)
        {
            const auto ai = static_cast<std::uint8_t>(arg);
            mBuffer.push_back(static_cast<std::uint8_t>((major_type << 5U) | ai));
            return;
        }

        if (arg <= std::numeric_limits<std::uint8_t>::max())
        {
            mBuffer.push_back(static_cast<std::uint8_t>((major_type << 5U) | 24U));
            mBuffer.push_back(static_cast<std::uint8_t>(arg));
            return;
        }

        if (arg <= std::numeric_limits<std::uint16_t>::max())
        {
            mBuffer.push_back(static_cast<std::uint8_t>((major_type << 5U) | 25U));
            mBuffer.push_back(static_cast<std::uint8_t>((arg >> 8) & 0xFFU));
            mBuffer.push_back(static_cast<std::uint8_t>(arg & 0xFFU));
            return;
        }

        if (arg <= std::numeric_limits<std::uint32_t>::max())
        {
            mBuffer.push_back(static_cast<std::uint8_t>((major_type << 5U) | 26U));
            for (int i = 3; i >= 0; --i)
            {
                mBuffer.push_back(static_cast<std::uint8_t>((arg >> (8 * i)) & 0xFFU));
            }
            return;
        }

        mBuffer.push_back(static_cast<std::uint8_t>((major_type << 5U) | 27U));
        for (int i = 7; i >= 0; --i)
        {
            mBuffer.push_back(static_cast<std::uint8_t>((arg >> (8 * i)) & 0xFFU));
        }
    }
};

// ============================================================================
// CborReader: Policy-free CBOR reader over contiguous bytes
// ============================================================================

class CborReader
{
public:
    CborReader(const std::uint8_t* data, std::size_t size) noexcept
        : data_(data)
        , size_(size)
        , mPos(0)
    {
    }

    template <typename Container, typename = std::enable_if_t<detail::is_byte_container_v<Container>>>
    explicit CborReader(const Container& buffer) noexcept
        : CborReader(buffer.data(), buffer.size())
    {
    }

    std::size_t remaining() const noexcept
    {
        return size_ - mPos;
    }

    bool empty() const noexcept
    {
        return remaining() == 0U;
    }

    CborResult<std::uint64_t> read_uint()
    {
        auto head = read_byte();
        if (!head)
        {
            return make_unexpected(CborError(head.error().message));
        }

        const std::uint8_t b = *head;
        const std::uint8_t mt = static_cast<std::uint8_t>(b >> 5U);
        const std::uint8_t ai = static_cast<std::uint8_t>(b & 0x1FU);

        if (mt != 0U)
        {
            return make_unexpected(CborError("Expected unsigned integer"));
        }

        auto arg = read_arg(ai);
        if (!arg)
        {
            return arg;
        }
        return *arg;
    }

    CborResult<std::int64_t> read_int()
    {
        auto head = read_byte();
        if (!head)
        {
            return make_unexpected(CborError(head.error().message));
        }

        const std::uint8_t b = *head;
        const std::uint8_t mt = static_cast<std::uint8_t>(b >> 5U);
        const std::uint8_t ai = static_cast<std::uint8_t>(b & 0x1FU);

        if (mt == 0U)
        {
            auto arg = read_arg(ai);
            if (!arg)
            {
                return make_unexpected(CborError(arg.error().message));
            }
            return static_cast<std::int64_t>(*arg);
        }

        if (mt == 1U)
        {
            auto arg = read_arg(ai);
            if (!arg)
            {
                return make_unexpected(CborError(arg.error().message));
            }
            return static_cast<std::int64_t>(-static_cast<std::int64_t>(*arg) - 1);
        }

        return make_unexpected(CborError("Expected integer"));
    }

    CborResult<bool> read_bool()
    {
        auto head = read_byte();
        if (!head)
        {
            return make_unexpected(CborError(head.error().message));
        }

        const std::uint8_t b = *head;
        if (b == 0xF4U)
        {
            return false;
        }
        if (b == 0xF5U)
        {
            return true;
        }
        return make_unexpected(CborError("Expected boolean"));
    }

    CborResult<double> read_double()
    {
        auto head = read_byte();
        if (!head)
        {
            return make_unexpected(CborError(head.error().message));
        }

        const std::uint8_t b = *head;
        const std::uint8_t mt = static_cast<std::uint8_t>(b >> 5U);
        const std::uint8_t ai = static_cast<std::uint8_t>(b & 0x1FU);

        if (mt != 7U || ai != 27U)
        {
            return make_unexpected(CborError("Expected 64-bit float"));
        }

        if (remaining() < 8U)
        {
            return make_unexpected(CborError("CBOR underflow reading double"));
        }

        std::uint64_t bits = 0;
        for (int i = 7; i >= 0; --i)
        {
            bits |= static_cast<std::uint64_t>(data_[mPos++]) << (8 * i);
        }

        double value = 0.0;
        static_assert(sizeof(value) == sizeof(bits), "Unexpected double size");
        std::memcpy(&value, &bits, sizeof(value));
        return value;
    }

    CborResult<std::string> read_string()
    {
        auto head = read_byte();
        if (!head)
        {
            return make_unexpected(CborError(head.error().message));
        }

        const std::uint8_t b = *head;
        const std::uint8_t mt = static_cast<std::uint8_t>(b >> 5U);
        const std::uint8_t ai = static_cast<std::uint8_t>(b & 0x1FU);

        if (mt != 3U)
        {
            return make_unexpected(CborError("Expected text string"));
        }

        auto len_res = read_arg(ai);
        if (!len_res)
        {
            return make_unexpected(CborError(len_res.error().message));
        }

        const std::uint64_t len = *len_res;
        if (len > remaining())
        {
            return make_unexpected(CborError("CBOR underflow reading string"));
        }

        std::string out;
        out.resize(static_cast<std::size_t>(len));
        std::memcpy(out.data(), data_ + mPos, static_cast<std::size_t>(len));
        mPos += static_cast<std::size_t>(len);
        return out;
    }

    CborResult<std::uint64_t> read_array_header()
    {
        return read_sized_header(4U, "array");
    }

    CborResult<std::uint64_t> read_map_header()
    {
        return read_sized_header(5U, "map");
    }

private:
    const std::uint8_t* data_;
    std::size_t size_;
    std::size_t mPos;

    CborResult<std::uint8_t> read_byte()
    {
        if (mPos >= size_)
        {
            return make_unexpected(CborError("CBOR underflow reading byte"));
        }
        return data_[mPos++];
    }

    CborResult<std::uint64_t> read_arg(std::uint8_t ai)
    {
        if (ai < 24U)
        {
            return ai;
        }

        if (remaining() == 0U)
        {
            return make_unexpected(CborError("CBOR underflow"));
        }

        if (ai == 24U)
        {
            return data_[mPos++];
        }

        if (ai == 25U)
        {
            if (remaining() < 2U)
            {
                return make_unexpected(CborError("CBOR underflow"));
            }
            const std::uint64_t v =
                (static_cast<std::uint64_t>(data_[mPos]) << 8U) | static_cast<std::uint64_t>(data_[mPos + 1U]);
            mPos += 2U;
            return v;
        }

        if (ai == 26U)
        {
            if (remaining() < 4U)
            {
                return make_unexpected(CborError("CBOR underflow"));
            }

            std::uint64_t v = 0;
            for (int i = 3; i >= 0; --i)
            {
                v |= static_cast<std::uint64_t>(data_[mPos++]) << (8 * i);
            }
            return v;
        }

        if (ai == 27U)
        {
            if (remaining() < 8U)
            {
                return make_unexpected(CborError("CBOR underflow"));
            }

            std::uint64_t v = 0;
            for (int i = 7; i >= 0; --i)
            {
                v |= static_cast<std::uint64_t>(data_[mPos++]) << (8 * i);
            }
            return v;
        }

        return make_unexpected(CborError("Invalid CBOR additional info"));
    }

    CborResult<std::uint64_t> read_sized_header(std::uint8_t expected_mt, const char* what)
    {
        auto head = read_byte();
        if (!head)
        {
            return make_unexpected(CborError(head.error().message));
        }

        const std::uint8_t b = *head;
        const std::uint8_t mt = static_cast<std::uint8_t>(b >> 5U);
        const std::uint8_t ai = static_cast<std::uint8_t>(b & 0x1FU);

        if (mt != expected_mt)
        {
            return make_unexpected(CborError(std::string("Expected ") + what + " header"));
        }

        return read_arg(ai);
    }
};

// ============================================================================
// CborTraits: Fat-P-style trait layer over CBOR
// ============================================================================

template <typename T, typename Enable = void>
struct CborTraits;

// Signed integers
template <typename T>
struct CborTraits<T, std::enable_if_t<std::is_integral_v<T> && std::is_signed_v<T>>>
{
    template <typename Writer>
    static void encode(Writer& writer, T value)
    {
        writer.write_int(value);
    }

    template <typename Reader>
    static CborResult<T> decode(Reader& reader)
    {
        auto v = reader.read_int();
        if (!v)
        {
            return make_unexpected(v.error());
        }

        if (*v < static_cast<std::int64_t>(std::numeric_limits<T>::min()) ||
            *v > static_cast<std::int64_t>(std::numeric_limits<T>::max()))
        {
            return make_unexpected(CborError("Integer out of range"));
        }

        return static_cast<T>(*v);
    }
};

// Unsigned integers
template <typename T>
struct CborTraits<T, std::enable_if_t<std::is_integral_v<T> && std::is_unsigned_v<T>>>
{
    template <typename Writer>
    static void encode(Writer& writer, T value)
    {
        writer.write_uint(value);
    }

    template <typename Reader>
    static CborResult<T> decode(Reader& reader)
    {
        auto v = reader.read_uint();
        if (!v)
        {
            return make_unexpected(v.error());
        }

        if (*v > std::numeric_limits<T>::max())
        {
            return make_unexpected(CborError("Unsigned integer out of range"));
        }

        return static_cast<T>(*v);
    }
};

// Enums (via underlying type)
template <typename T>
struct CborTraits<T, std::enable_if_t<std::is_enum_v<T>>>
{
    template <typename Writer>
    static void encode(Writer& writer, T value)
    {
        using underlying = std::underlying_type_t<T>;
        CborTraits<underlying>::encode(writer, static_cast<underlying>(value));
    }

    template <typename Reader>
    static CborResult<T> decode(Reader& reader)
    {
        using underlying = std::underlying_type_t<T>;
        auto v = CborTraits<underlying>::decode(reader);
        if (!v)
        {
            return make_unexpected(CborError(v.error().message));
        }
        return static_cast<T>(*v);
    }
};

// bool
template <>
struct CborTraits<bool>
{
    template <typename Writer>
    static void encode(Writer& writer, bool value)
    {
        writer.write_bool(value);
    }

    template <typename Reader>
    static CborResult<bool> decode(Reader& reader)
    {
        return reader.read_bool();
    }
};

// double
template <>
struct CborTraits<double>
{
    template <typename Writer>
    static void encode(Writer& writer, double value)
    {
        writer.write_double(value);
    }

    template <typename Reader>
    static CborResult<double> decode(Reader& reader)
    {
        return reader.read_double();
    }
};

// std::string
template <>
struct CborTraits<std::string>
{
    template <typename Writer>
    static void encode(Writer& writer, const std::string& value)
    {
        writer.write_string(value);
    }

    template <typename Reader>
    static CborResult<std::string> decode(Reader& reader)
    {
        return reader.read_string();
    }
};

// std::vector
template <typename T, typename Alloc>
struct CborTraits<std::vector<T, Alloc>>
{
    template <typename Writer>
    static void encode(Writer& writer, const std::vector<T, Alloc>& vec)
    {
        writer.write_array_header(static_cast<std::uint64_t>(vec.size()));
        for (const auto& elem : vec)
        {
            CborTraits<T>::encode(writer, elem);
        }
    }

    template <typename Reader>
    static CborResult<std::vector<T, Alloc>> decode(Reader& reader)
    {
        auto len_res = reader.read_array_header();
        if (!len_res)
        {
            return make_unexpected(CborError(len_res.error().message));
        }

        const std::uint64_t len = *len_res;
        if (len > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max()))
        {
            return make_unexpected(CborError("Array length too large"));
        }

        std::vector<T, Alloc> result;
        result.reserve(static_cast<std::size_t>(len));
        for (std::uint64_t i = 0; i < len; ++i)
        {
            auto elem = CborTraits<T>::decode(reader);
            if (!elem)
            {
                return make_unexpected(CborError(elem.error().message));
            }
            result.push_back(std::move(*elem));
        }
        return result;
    }
};

// std::map
template <typename K, typename V, typename Compare, typename Alloc>
struct CborTraits<std::map<K, V, Compare, Alloc>>
{
    template <typename Writer>
    static void encode(Writer& writer, const std::map<K, V, Compare, Alloc>& m)
    {
        writer.write_map_header(static_cast<std::uint64_t>(m.size()));
        for (const auto& kv : m)
        {
            CborTraits<K>::encode(writer, kv.first);
            CborTraits<V>::encode(writer, kv.second);
        }
    }

    template <typename Reader>
    static CborResult<std::map<K, V, Compare, Alloc>> decode(Reader& reader)
    {
        auto len_res = reader.read_map_header();
        if (!len_res)
        {
            return make_unexpected(CborError(len_res.error().message));
        }

        const std::uint64_t len = *len_res;
        if (len > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max()))
        {
            return make_unexpected(CborError("Map length too large"));
        }

        std::map<K, V, Compare, Alloc> result;
        for (std::uint64_t i = 0; i < len; ++i)
        {
            auto key = CborTraits<K>::decode(reader);
            if (!key)
            {
                return make_unexpected(CborError(key.error().message));
            }

            auto value = CborTraits<V>::decode(reader);
            if (!value)
            {
                return make_unexpected(CborError(value.error().message));
            }

            result.emplace(std::move(*key), std::move(*value));
        }

        return result;
    }
};

// ============================================================================
// Free-function frontends
// ============================================================================

template <typename T, typename Writer>
void cbor_encode(Writer& writer, const T& value)
{
    CborTraits<T>::encode(writer, value);
}

template <typename T, typename Reader>
CborResult<T> cbor_decode(Reader& reader)
{
    return CborTraits<T>::decode(reader);
}

template <typename T, typename ByteContainer>
CborResult<void> cbor_encode_to(ByteContainer& buffer, const T& value)
{
    static_assert(detail::is_byte_container_v<ByteContainer>,
                  "ByteContainer must store uint8_t and provide data()/size()/push_back()");

    CborWriter<ByteContainer> writer(buffer);
    try
    {
        CborTraits<T>::encode(writer, value);
    }
    catch (const std::exception& ex)
    {
        return make_unexpected(CborError(ex.what()));
    }
    return {};
}

template <typename T, typename ByteContainer>
CborResult<T> cbor_decode_from(const ByteContainer& buffer)
{
    static_assert(detail::is_byte_container_v<ByteContainer>,
                  "ByteContainer must store uint8_t and provide data()/size()/push_back()");

    CborReader reader(buffer);
    try
    {
        return CborTraits<T>::decode(reader);
    }
    catch (const std::exception& ex)
    {
        return make_unexpected(CborError(ex.what()));
    }
}

} // namespace cbor_fatp
} // namespace fat_p

// Local-scope convenience imports (does not pollute namespace fat_p)
#define USING_FATP_CBOR()                      \
    using fat_p::cbor_fatp::CborError;         \
    using fat_p::cbor_fatp::CborResult;        \
    using fat_p::cbor_fatp::DefaultCborBuffer; \
    using fat_p::cbor_fatp::CborBuffer;        \
    using fat_p::cbor_fatp::CborWriter;        \
    using fat_p::cbor_fatp::CborReader;        \
    using fat_p::cbor_fatp::CborTraits;        \
    using fat_p::cbor_fatp::cbor_encode;       \
    using fat_p::cbor_fatp::cbor_decode;       \
    using fat_p::cbor_fatp::cbor_encode_to;    \
    using fat_p::cbor_fatp::cbor_decode_from;
