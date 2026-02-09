#pragma once

/*
FATP_META:
  meta_version: 1
  component: CborLite
  file_role: public_header
  path: include/fat_p/CborLite.h
  namespace: fat_p
  layer: Foundation
  summary: "Public header for CborLite."
  api_stability: in_work
  related:
    docs_search: "CborLite"
    tests:
      - components/Cbor/tests/test_CborLite.cpp
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
 * @file CborLite.h
 * @brief Minimal CBOR encoder/decoder with no Fat-P dependencies beyond the namespace.
 * C++20, header-only, standard library only.
 */

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

inline void writeTypeAndArgument(buffer& out, MajorType mt, std::uint64_t arg)
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

inline void ensureAvailable(std::size_t size, std::size_t pos, std::size_t required)
{
    if (pos + required > size)
    {
        throw std::runtime_error("CBOR: truncated input");
    }
}

inline std::size_t safeToSizeT(std::uint64_t value, const char* context)
{
    if (value > std::numeric_limits<std::size_t>::max())
    {
        throw std::runtime_error(std::string("CBOR: ") + context + " length exceeds platform limits");
    }
    return static_cast<std::size_t>(value);
}

inline std::uint64_t readArgument(const byte* data, std::size_t& pos, std::size_t size, std::uint8_t ai)
{
    if (ai < 24U)
    {
        return ai;
    }

    if (ai == 24U)
    {
        ensureAvailable(size, pos, 1U);
        const auto value = data[pos];
        ++pos;
        return value;
    }

    if (ai == 25U)
    {
        ensureAvailable(size, pos, 2U);
        const std::uint64_t result =
            (static_cast<std::uint64_t>(data[pos]) << 8U) | static_cast<std::uint64_t>(data[pos + 1U]);
        pos += 2U;
        return result;
    }

    if (ai == 26U)
    {
        ensureAvailable(size, pos, 4U);
        std::uint64_t result = 0;
        for (std::size_t i = 0; i < 4; ++i)
        {
            result = (result << 8U) | static_cast<std::uint64_t>(data[pos + i]);
        }
        pos += 4U;
        return result;
    }

    if (ai == 27U)
    {
        ensureAvailable(size, pos, 8U);
        std::uint64_t result = 0;
        for (std::size_t i = 0; i < 8; ++i)
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
    explicit Encoder(buffer& out) noexcept
        : mOut(out)
    {
    }

    buffer& output() noexcept
    {
        return mOut;
    }

    void writeUint(std::uint64_t value)
    {
        writeTypeAndArgument(mOut, MajorType::UnsignedInt, value);
    }

    void writeInt(std::int64_t value)
    {
        if (value >= 0)
        {
            writeUint(static_cast<std::uint64_t>(value));
        }
        else
        {
            // Use bitwise complement to avoid signed overflow when value == INT64_MIN.
            // For negative integers, CBOR encodes -1-n where n is stored.
            // ~value == -value - 1 for two's complement, but without overflow.
            const auto n = static_cast<std::uint64_t>(~value);
            writeTypeAndArgument(mOut, MajorType::NegativeInt, n);
        }
    }

    void writeBool(bool value)
    {
        const std::uint8_t base = 0xF4U; // false
        const std::uint8_t offset = value ? 1U : 0U;
        mOut.push_back(static_cast<byte>(base + offset));
    }

    void writeNull()
    {
        mOut.push_back(static_cast<byte>(0xF6U));
    }

    void writeBytes(const byte* data, std::size_t size)
    {
        writeTypeAndArgument(mOut, MajorType::ByteString, static_cast<std::uint64_t>(size));
        mOut.insert(mOut.end(), data, data + size);
    }

    void writeBytes(const buffer& b)
    {
        if (!b.empty())
        {
            writeBytes(b.data(), b.size());
        }
        else
        {
            writeTypeAndArgument(mOut, MajorType::ByteString, 0U);
        }
    }

    void writeText(const std::string& s)
    {
        writeTypeAndArgument(mOut, MajorType::TextString, static_cast<std::uint64_t>(s.size()));
        mOut.insert(mOut.end(), s.begin(), s.end());
    }

    void beginArray(std::size_t size)
    {
        writeTypeAndArgument(mOut, MajorType::Array, static_cast<std::uint64_t>(size));
    }

    void beginMap(std::size_t size)
    {
        writeTypeAndArgument(mOut, MajorType::Map, static_cast<std::uint64_t>(size));
    }

private:
    buffer& mOut;
};

// ----------------------------------------------------------------------------
// Decoder
// ----------------------------------------------------------------------------

class Decoder
{
public:
    /// Default maximum nesting depth for arrays/maps (matches JsonLite policy)
    static constexpr std::size_t kDefaultMaxDepth = 64;

    Decoder(const byte* data, std::size_t size) noexcept
        : mData(data)
        , mSize(size)
        , mPos(0)
    {
    }

    explicit Decoder(const buffer& b) noexcept
        : mData(b.data())
        , mSize(b.size())
        , mPos(0)
    {
    }

    /// Set maximum nesting depth for array/map decoding (default: 64)
    void set_max_depth(std::size_t depth) noexcept
    {
        mMaxDepth = depth;
    }

    /// Get current nesting depth
    [[nodiscard]] std::size_t depth() const noexcept
    {
        return mDepth;
    }

    bool eof() const noexcept
    {
        return mPos >= mSize;
    }

    ItemHeader readHeader()
    {
        ensureAvailable(mSize, mPos, 1U);
        const std::uint8_t initial = mData[mPos++];

        const std::uint8_t majorBits = initial >> 5U;
        const std::uint8_t ai = initial & 0x1FU;

        ItemHeader header{};
        header.major = static_cast<MajorType>(majorBits);
        header.argument = readArgument(mData, mPos, mSize, ai);
        return header;
    }

    std::uint64_t readUint()
    {
        const ItemHeader header = readHeader();
        if (header.major != MajorType::UnsignedInt)
        {
            throw std::runtime_error("CBOR: expected unsigned integer");
        }
        return header.argument;
    }

    std::int64_t readInt()
    {
        const ItemHeader header = readHeader();

        if (header.major == MajorType::UnsignedInt)
        {
            const auto max = static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
            if (header.argument > max)
            {
                throw std::runtime_error("CBOR: unsigned integer too large for int64_t");
            }
            return static_cast<std::int64_t>(header.argument);
        }

        if (header.major == MajorType::NegativeInt)
        {
            const auto max = static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
            if (header.argument > max)
            {
                throw std::runtime_error("CBOR: negative integer too small for int64_t");
            }
            const auto n = static_cast<std::int64_t>(header.argument);
            return -1 - n;
        }

        throw std::runtime_error("CBOR: expected integer");
    }

    bool readBool()
    {
        ensureAvailable(mSize, mPos, 1U);
        const std::uint8_t v = mData[mPos++];
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

    void readNull()
    {
        ensureAvailable(mSize, mPos, 1U);
        const std::uint8_t v = mData[mPos++];
        if (v != 0xF6U)
        {
            throw std::runtime_error("CBOR: expected null");
        }
    }

    std::string readText()
    {
        const ItemHeader header = readHeader();
        if (header.major != MajorType::TextString)
        {
            throw std::runtime_error("CBOR: expected text string");
        }

        const auto len = safeToSizeT(header.argument, "text string");
        ensureAvailable(mSize, mPos, len);

        const char* begin = reinterpret_cast<const char*>(mData + mPos);
        std::string result(begin, begin + len);
        mPos += len;
        return result;
    }

    buffer readBytes()
    {
        const ItemHeader header = readHeader();
        if (header.major != MajorType::ByteString)
        {
            throw std::runtime_error("CBOR: expected byte string");
        }

        const auto len = safeToSizeT(header.argument, "byte string");
        ensureAvailable(mSize, mPos, len);

        buffer out;
        out.insert(out.end(), mData + mPos, mData + mPos + len);
        mPos += len;
        return out;
    }

    /**
     * @brief Read array length with nesting depth enforcement
     * @throws std::runtime_error if nesting exceeds max_depth
     */
    std::size_t readArrayLength()
    {
        const ItemHeader header = readHeader();
        if (header.major != MajorType::Array)
        {
            throw std::runtime_error("CBOR: expected array");
        }
        if (mDepth >= mMaxDepth)
        {
            throw std::runtime_error("CBOR: maximum nesting depth exceeded");
        }
        ++mDepth;
        return safeToSizeT(header.argument, "array");
    }

    /**
     * @brief Read map length with nesting depth enforcement
     * @throws std::runtime_error if nesting exceeds max_depth
     */
    std::size_t readMapLength()
    {
        const ItemHeader header = readHeader();
        if (header.major != MajorType::Map)
        {
            throw std::runtime_error("CBOR: expected map");
        }
        if (mDepth >= mMaxDepth)
        {
            throw std::runtime_error("CBOR: maximum nesting depth exceeded");
        }
        ++mDepth;
        return safeToSizeT(header.argument, "map");
    }

    /**
     * @brief Notify decoder that a container (array/map) has been fully consumed
     *
     * Call after iterating all elements of an array or map to restore depth.
     * Forgetting to call this is safe (depth only increases), but may cause
     * spurious depth-exceeded errors on subsequent containers.
     */
    void endContainer() noexcept
    {
        if (mDepth > 0)
        {
            --mDepth;
        }
    }

private:
    const byte* mData;
    std::size_t mSize;
    std::size_t mPos;
    std::size_t mDepth = 0;
    std::size_t mMaxDepth = kDefaultMaxDepth;
};

} // namespace cbor
} // namespace fat_p
