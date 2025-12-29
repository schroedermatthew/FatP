#pragma once

// CborLite.h
// Minimal CBOR encoder/decoder with no Fat-P dependencies beyond the namespace.
// C++17, header-only, standard library only.

#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace fat_p
{
namespace cbor
{

using byte = std::uint8_t;
using buffer = std::vector<byte>;

// ----------------------------------------------------------------------------
// Major CBOR types (RFC 8949)
// ----------------------------------------------------------------------------

enum class MajorType : std::uint8_t
{
    UnsignedInt = 0,
    NegativeInt = 1,
    ByteString = 2,
    TextString = 3,
    Array = 4,
    Map = 5,
    Tag = 6,
    Simple = 7
};

struct ItemHeader
{
    MajorType major = MajorType::UnsignedInt;
    std::uint64_t argument = 0; // value or length
};

// ----------------------------------------------------------------------------
// Low-level helpers
// ----------------------------------------------------------------------------

inline void write_type_and_argument(buffer &out, MajorType mt, std::uint64_t arg)
{
    const std::uint8_t major = static_cast<std::uint8_t>(mt) << 5U;

    if (arg <= 23U)
    {
        const auto ai = static_cast<std::uint8_t>(arg);
        out.push_back(static_cast<byte>(major | ai));
        return;
    }

    if (arg <= 0xFFU)
    {
        out.push_back(static_cast<byte>(major | 24U));
        out.push_back(static_cast<byte>(arg));
        return;
    }

    if (arg <= 0xFFFFU)
    {
        out.push_back(static_cast<byte>(major | 25U));
        out.push_back(static_cast<byte>((arg >> 8U) & 0xFFU));
        out.push_back(static_cast<byte>(arg & 0xFFU));
        return;
    }

    if (arg <= 0xFFFFFFFFULL)
    {
        out.push_back(static_cast<byte>(major | 26U));
        out.push_back(static_cast<byte>((arg >> 24U) & 0xFFU));
        out.push_back(static_cast<byte>((arg >> 16U) & 0xFFU));
        out.push_back(static_cast<byte>((arg >> 8U) & 0xFFU));
        out.push_back(static_cast<byte>(arg & 0xFFU));
        return;
    }

    out.push_back(static_cast<byte>(major | 27U));
    out.push_back(static_cast<byte>((arg >> 56U) & 0xFFU));
    out.push_back(static_cast<byte>((arg >> 48U) & 0xFFU));
    out.push_back(static_cast<byte>((arg >> 40U) & 0xFFU));
    out.push_back(static_cast<byte>((arg >> 32U) & 0xFFU));
    out.push_back(static_cast<byte>((arg >> 24U) & 0xFFU));
    out.push_back(static_cast<byte>((arg >> 16U) & 0xFFU));
    out.push_back(static_cast<byte>((arg >> 8U) & 0xFFU));
    out.push_back(static_cast<byte>(arg & 0xFFU));
}

inline void ensure_available(std::size_t size,
                             std::size_t pos,
                             std::size_t required)
{
    if (pos + required > size)
    {
        throw std::runtime_error("CBOR: truncated input");
    }
}

inline std::size_t safe_to_size_t(std::uint64_t value, const char* context)
{
    if (value > std::numeric_limits<std::size_t>::max())
    {
        throw std::runtime_error(std::string("CBOR: ") + context +
                                 " length exceeds platform limits");
    }
    return static_cast<std::size_t>(value);
}

inline std::uint64_t read_argument(const byte *data,
                                   std::size_t &pos,
                                   std::size_t size,
                                   std::uint8_t ai)
{
    if (ai < 24U)
    {
        return ai;
    }

    if (ai == 24U)
    {
        ensure_available(size, pos, 1U);
        const auto value = data[pos];
        ++pos;
        return value;
    }

    if (ai == 25U)
    {
        ensure_available(size, pos, 2U);
        const std::uint64_t result =
            (static_cast<std::uint64_t>(data[pos]) << 8U) |
            static_cast<std::uint64_t>(data[pos + 1U]);
        pos += 2U;
        return result;
    }

    if (ai == 26U)
    {
        ensure_available(size, pos, 4U);
        std::uint64_t result = 0;
        for (int i = 0; i < 4; ++i)
        {
            result = (result << 8U) | static_cast<std::uint64_t>(data[pos + i]);
        }
        pos += 4U;
        return result;
    }

    if (ai == 27U)
    {
        ensure_available(size, pos, 8U);
        std::uint64_t result = 0;
        for (int i = 0; i < 8; ++i)
        {
            result = (result << 8U) | static_cast<std::uint64_t>(data[pos + i]);
        }
        pos += 8U;
        return result;
    }

    throw std::runtime_error("CBOR: indefinite lengths not supported");
}

// ----------------------------------------------------------------------------
// Encoder
// ----------------------------------------------------------------------------

class Encoder
{
public:
    explicit Encoder(buffer &out) noexcept
        : out_(out)
    {
    }

    buffer &output() noexcept
    {
        return out_;
    }

    void write_uint(std::uint64_t value)
    {
        write_type_and_argument(out_, MajorType::UnsignedInt, value);
    }

    void write_int(std::int64_t value)
    {
        if (value >= 0)
        {
            write_uint(static_cast<std::uint64_t>(value));
        }
        else
        {
            // Use bitwise complement to avoid signed overflow when value == INT64_MIN.
            // For negative integers, CBOR encodes -1-n where n is stored.
            // ~value == -value - 1 for two's complement, but without overflow.
            const auto n = static_cast<std::uint64_t>(~value);
            write_type_and_argument(out_, MajorType::NegativeInt, n);
        }
    }

    void write_bool(bool value)
    {
        const std::uint8_t base = 0xF4U; // false
        const std::uint8_t offset = value ? 1U : 0U;
        out_.push_back(static_cast<byte>(base + offset));
    }

    void write_null()
    {
        out_.push_back(static_cast<byte>(0xF6U));
    }

    void write_bytes(const byte *data, std::size_t size)
    {
        write_type_and_argument(out_, MajorType::ByteString,
                                static_cast<std::uint64_t>(size));
        out_.insert(out_.end(), data, data + size);
    }

    void write_bytes(const buffer &b)
    {
        if (!b.empty())
        {
            write_bytes(b.data(), b.size());
        }
        else
        {
            write_type_and_argument(out_, MajorType::ByteString, 0U);
        }
    }

    void write_text(const std::string &s)
    {
        write_type_and_argument(out_, MajorType::TextString,
                                static_cast<std::uint64_t>(s.size()));
        out_.insert(out_.end(), s.begin(), s.end());
    }

    void begin_array(std::size_t size)
    {
        write_type_and_argument(out_, MajorType::Array,
                                static_cast<std::uint64_t>(size));
    }

    void begin_map(std::size_t size)
    {
        write_type_and_argument(out_, MajorType::Map,
                                static_cast<std::uint64_t>(size));
    }

private:
    buffer &out_;
};

// ----------------------------------------------------------------------------
// Decoder
// ----------------------------------------------------------------------------

class Decoder
{
public:
    Decoder(const byte *data, std::size_t size) noexcept
        : data_(data)
        , size_(size)
        , pos_(0)
    {
    }

    explicit Decoder(const buffer &b) noexcept
        : data_(b.data())
        , size_(b.size())
        , pos_(0)
    {
    }

    bool eof() const noexcept
    {
        return pos_ >= size_;
    }

    ItemHeader read_header()
    {
        ensure_available(size_, pos_, 1U);
        const std::uint8_t initial = data_[pos_++];

        const std::uint8_t major_bits = initial >> 5U;
        const std::uint8_t ai = initial & 0x1FU;

        ItemHeader header{};
        header.major = static_cast<MajorType>(major_bits);
        header.argument = read_argument(data_, pos_, size_, ai);
        return header;
    }

    std::uint64_t read_uint()
    {
        const ItemHeader header = read_header();
        if (header.major != MajorType::UnsignedInt)
        {
            throw std::runtime_error("CBOR: expected unsigned integer");
        }
        return header.argument;
    }

    std::int64_t read_int()
    {
        const ItemHeader header = read_header();

        if (header.major == MajorType::UnsignedInt)
        {
            const auto max = static_cast<std::uint64_t>(
                std::numeric_limits<std::int64_t>::max());
            if (header.argument > max)
            {
                throw std::runtime_error("CBOR: unsigned integer too large for int64_t");
            }
            return static_cast<std::int64_t>(header.argument);
        }

        if (header.major == MajorType::NegativeInt)
        {
            const auto max = static_cast<std::uint64_t>(
                std::numeric_limits<std::int64_t>::max());
            if (header.argument > max)
            {
                throw std::runtime_error("CBOR: negative integer too small for int64_t");
            }
            const auto n = static_cast<std::int64_t>(header.argument);
            return -1 - n;
        }

        throw std::runtime_error("CBOR: expected integer");
    }

    bool read_bool()
    {
        ensure_available(size_, pos_, 1U);
        const std::uint8_t v = data_[pos_++];
        if (v == 0xF4U)
        {
            return false;
        }
        if (v == 0xF5U)
        {
            return true;
        }
        throw std::runtime_error("CBOR: expected bool");
    }

    void read_null()
    {
        ensure_available(size_, pos_, 1U);
        const std::uint8_t v = data_[pos_++];
        if (v != 0xF6U)
        {
            throw std::runtime_error("CBOR: expected null");
        }
    }

    std::string read_text()
    {
        const ItemHeader header = read_header();
        if (header.major != MajorType::TextString)
        {
            throw std::runtime_error("CBOR: expected text string");
        }

        const auto len = safe_to_size_t(header.argument, "text string");
        ensure_available(size_, pos_, len);

        const char *begin = reinterpret_cast<const char *>(data_ + pos_);
        std::string result(begin, begin + len);
        pos_ += len;
        return result;
    }

    buffer read_bytes()
    {
        const ItemHeader header = read_header();
        if (header.major != MajorType::ByteString)
        {
            throw std::runtime_error("CBOR: expected byte string");
        }

        const auto len = safe_to_size_t(header.argument, "byte string");
        ensure_available(size_, pos_, len);

        buffer out;
        out.insert(out.end(), data_ + pos_, data_ + pos_ + len);
        pos_ += len;
        return out;
    }

    std::size_t read_array_length()
    {
        const ItemHeader header = read_header();
        if (header.major != MajorType::Array)
        {
            throw std::runtime_error("CBOR: expected array");
        }
        return safe_to_size_t(header.argument, "array");
    }

    std::size_t read_map_length()
    {
        const ItemHeader header = read_header();
        if (header.major != MajorType::Map)
        {
            throw std::runtime_error("CBOR: expected map");
        }
        return safe_to_size_t(header.argument, "map");
    }

private:
    const byte *data_;
    std::size_t size_;
    std::size_t pos_;
};

} // namespace cbor
} // namespace fat_p
