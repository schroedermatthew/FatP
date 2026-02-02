#pragma once

/*
FATP_META:
  meta_version: 1
  component: BinaryLite
  file_role: public_header
  path: include/fat_p/BinaryLite.h
  namespace: fat_p
  layer: Foundation
  summary: "Public header for BinaryLite."
  api_stability: in_work
  related:
    docs_search: "BinaryLite"
    tests:
      - components/BinarySerialization/tests/test_BinaryLite.cpp
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
 * @file BinaryLite.h
 * @brief Lightweight binary serialization for FAT-P types
 *
 * @details
 * Lightweight binary serialization library - standalone with minimal dependencies
 * Simple little-endian format optimized for speed and simplicity
 * C++20, header-only
 * This is the STANDALONE version with minimal external dependencies.
 * For fat_p component integration (Expected-based API, HpcVector buffers,
 * fat_p type serialization), use FatPBinary.h instead.
 */

#include <cstdint>
#include <cstring>
#include <istream>
#include <limits>
#include <ostream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

#ifdef __AVX2__
#include <immintrin.h>
#endif

namespace fat_p
{
namespace binary
{

// ============================================================================
// Endianness Detection and Byte Swapping
// ============================================================================

namespace detail
{

// Compile-time endianness detection
// C++20 has std::endian; for C++17 we use compiler intrinsics
#if defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) && defined(__ORDER_BIG_ENDIAN__)
// GCC, Clang, and most modern compilers
inline constexpr bool kIsLittleEndian = (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__);
inline constexpr bool kIsBigEndian = (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__);
#elif defined(_MSC_VER)
// MSVC: x86/x64 is always little-endian
inline constexpr bool kIsLittleEndian = true;
inline constexpr bool kIsBigEndian = false;
#else
// Fallback: runtime detection (evaluated once at startup)
inline const bool kIsLittleEndian = []() {
    const std::uint32_t test = 0x01020304;
    return *reinterpret_cast<const std::uint8_t*>(&test) == 0x04;
}();
inline const bool kIsBigEndian = !kIsLittleEndian;
#endif

// Byte swap functions for converting between native and little-endian
inline std::uint8_t byteSwap(std::uint8_t v) noexcept
{
    return v;
}
inline std::int8_t byteSwap(std::int8_t v) noexcept
{
    return v;
}

inline std::uint16_t byteSwap(std::uint16_t v) noexcept
{
#if defined(_MSC_VER)
    return _byteswap_ushort(v);
#elif defined(__GNUC__) || defined(__clang__)
    return __builtin_bswap16(v);
#else
    return static_cast<std::uint16_t>((v >> 8) | (v << 8));
#endif
}

inline std::int16_t byteSwap(std::int16_t v) noexcept
{
    return static_cast<std::int16_t>(byteSwap(static_cast<std::uint16_t>(v)));
}

inline std::uint32_t byteSwap(std::uint32_t v) noexcept
{
#if defined(_MSC_VER)
    return _byteswap_ulong(v);
#elif defined(__GNUC__) || defined(__clang__)
    return __builtin_bswap32(v);
#else
    return ((v & 0xFF000000u) >> 24) | ((v & 0x00FF0000u) >> 8) | ((v & 0x0000FF00u) << 8) | ((v & 0x000000FFu) << 24);
#endif
}

inline std::int32_t byteSwap(std::int32_t v) noexcept
{
    return static_cast<std::int32_t>(byteSwap(static_cast<std::uint32_t>(v)));
}

inline std::uint64_t byteSwap(std::uint64_t v) noexcept
{
#if defined(_MSC_VER)
    return _byteswap_uint64(v);
#elif defined(__GNUC__) || defined(__clang__)
    return __builtin_bswap64(v);
#else
    return ((v & 0xFF00000000000000ull) >> 56) | ((v & 0x00FF000000000000ull) >> 40) |
           ((v & 0x0000FF0000000000ull) >> 24) | ((v & 0x000000FF00000000ull) >> 8) |
           ((v & 0x00000000FF000000ull) << 8) | ((v & 0x0000000000FF0000ull) << 24) |
           ((v & 0x000000000000FF00ull) << 40) | ((v & 0x00000000000000FFull) << 56);
#endif
}

inline std::int64_t byteSwap(std::int64_t v) noexcept
{
    return static_cast<std::int64_t>(byteSwap(static_cast<std::uint64_t>(v)));
}

// Float/double byte swapping via type punning
inline float byteSwap(float v) noexcept
{
    static_assert(sizeof(float) == sizeof(std::uint32_t), "float must be 32-bit");
    std::uint32_t bits;
    std::memcpy(&bits, &v, sizeof(bits));
    bits = byteSwap(bits);
    float result;
    std::memcpy(&result, &bits, sizeof(result));
    return result;
}

inline double byteSwap(double v) noexcept
{
    static_assert(sizeof(double) == sizeof(std::uint64_t), "double must be 64-bit");
    std::uint64_t bits;
    std::memcpy(&bits, &v, sizeof(bits));
    bits = byteSwap(bits);
    double result;
    std::memcpy(&result, &bits, sizeof(result));
    return result;
}

// Convert native to little-endian
template <typename T>
T nativeToLe(T v) noexcept
{
    if constexpr (sizeof(T) == 1)
    {
        return v;
    }
    else
    {
        if (kIsBigEndian)
        {
            return byteSwap(v);
        }
        return v;
    }
}

// Convert little-endian to native
template <typename T>
T leToNative(T v) noexcept
{
    if constexpr (sizeof(T) == 1)
    {
        return v;
    }
    else
    {
        if (kIsBigEndian)
        {
            return byteSwap(v);
        }
        return v;
    }
}

} // namespace detail

// ============================================================================
// Type Tags for Self-Describing Format
// ============================================================================

enum class TypeTag : std::uint8_t
{
    Uint8 = 0,
    Uint16 = 1,
    Uint32 = 2,
    Uint64 = 3,
    Int8 = 4,
    Int16 = 5,
    Int32 = 6,
    Int64 = 7,
    Float32 = 8,
    Float64 = 9,
    Bool = 10,
    String = 11,
    Bytes = 12,
    Array = 13,
    Map = 14
};

// ============================================================================
// Low-Level Encoding/Decoding Helpers
// ============================================================================

inline void ensureAvailable(std::size_t size, std::size_t pos, std::size_t required)
{
    if (pos + required > size)
    {
        throw std::runtime_error("Binary: buffer underflow");
    }
}

inline std::size_t safeToSizeT(std::uint64_t value, const char* context)
{
    if (value > std::numeric_limits<std::size_t>::max())
    {
        throw std::runtime_error(std::string("Binary: ") + context + " length exceeds platform limits");
    }
    return static_cast<std::size_t>(value);
}

template <typename T>
void writeLe(std::vector<std::uint8_t>& buffer, T value)
{
    static_assert(std::is_trivially_copyable_v<T>, "Type must be trivially copyable");

    // Convert to little-endian before writing
    const T leValue = detail::nativeToLe(value);
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(&leValue);
    buffer.insert(buffer.end(), bytes, bytes + sizeof(T));
}

template <typename T>
T readLe(const std::uint8_t* data, std::size_t& pos, std::size_t size)
{
    static_assert(std::is_trivially_copyable_v<T>, "Type must be trivially copyable");

    ensureAvailable(size, pos, sizeof(T));

    T leValue;
    std::memcpy(&leValue, data + pos, sizeof(T));
    pos += sizeof(T);
    // Convert from little-endian to native
    return detail::leToNative(leValue);
}

inline void copyData(std::vector<std::uint8_t>& buffer, const std::uint8_t* src, std::size_t len)
{
#ifdef __AVX2__
    const std::size_t oldSize = buffer.size();
    buffer.resize(oldSize + len);
    std::uint8_t* dst = buffer.data() + oldSize;

    std::size_t i = 0;
    for (; i + 32 <= len; i += 32)
    {
        __m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(src + i));
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(dst + i), chunk);
    }
    std::memcpy(dst + i, src + i, len - i);
#else
    buffer.insert(buffer.end(), src, src + len);
#endif
}

// ============================================================================
// Encoder
// ============================================================================

class Encoder
{
public:
    using buffer = std::vector<std::uint8_t>;

    explicit Encoder(buffer& out) noexcept
        : mOut(out)
    {
    }

    buffer& output() noexcept
    {
        return mOut;
    }

    void writeUint8(std::uint8_t value)
    {
        mOut.push_back(static_cast<std::uint8_t>(TypeTag::Uint8));
        mOut.push_back(value);
    }

    void writeUint16(std::uint16_t value)
    {
        mOut.push_back(static_cast<std::uint8_t>(TypeTag::Uint16));
        writeLe(mOut, value);
    }

    void writeUint32(std::uint32_t value)
    {
        mOut.push_back(static_cast<std::uint8_t>(TypeTag::Uint32));
        writeLe(mOut, value);
    }

    void writeUint64(std::uint64_t value)
    {
        mOut.push_back(static_cast<std::uint8_t>(TypeTag::Uint64));
        writeLe(mOut, value);
    }

    void writeInt8(std::int8_t value)
    {
        mOut.push_back(static_cast<std::uint8_t>(TypeTag::Int8));
        mOut.push_back(static_cast<std::uint8_t>(value));
    }

    void writeInt16(std::int16_t value)
    {
        mOut.push_back(static_cast<std::uint8_t>(TypeTag::Int16));
        writeLe(mOut, value);
    }

    void writeInt32(std::int32_t value)
    {
        mOut.push_back(static_cast<std::uint8_t>(TypeTag::Int32));
        writeLe(mOut, value);
    }

    void writeInt64(std::int64_t value)
    {
        mOut.push_back(static_cast<std::uint8_t>(TypeTag::Int64));
        writeLe(mOut, value);
    }

    void writeFloat(float value)
    {
        mOut.push_back(static_cast<std::uint8_t>(TypeTag::Float32));
        writeLe(mOut, value);
    }

    void writeDouble(double value)
    {
        mOut.push_back(static_cast<std::uint8_t>(TypeTag::Float64));
        writeLe(mOut, value);
    }

    void writeBool(bool value)
    {
        mOut.push_back(static_cast<std::uint8_t>(TypeTag::Bool));
        mOut.push_back(value ? 1U : 0U);
    }

    void writeString(const std::string& value)
    {
        mOut.push_back(static_cast<std::uint8_t>(TypeTag::String));
        writeLe(mOut, static_cast<std::uint64_t>(value.size()));
        copyData(mOut, reinterpret_cast<const std::uint8_t*>(value.data()), value.size());
    }

    void writeBytes(const std::uint8_t* data, std::size_t size)
    {
        mOut.push_back(static_cast<std::uint8_t>(TypeTag::Bytes));
        writeLe(mOut, static_cast<std::uint64_t>(size));
        copyData(mOut, data, size);
    }

    void writeBytes(const std::vector<std::uint8_t>& data)
    {
        writeBytes(data.data(), data.size());
    }

    void beginArray(std::size_t size)
    {
        mOut.push_back(static_cast<std::uint8_t>(TypeTag::Array));
        writeLe(mOut, static_cast<std::uint64_t>(size));
    }

    void beginMap(std::size_t size)
    {
        mOut.push_back(static_cast<std::uint8_t>(TypeTag::Map));
        writeLe(mOut, static_cast<std::uint64_t>(size));
    }

    // Untagged writes for when caller manages type information
    template <typename T>
    void writeRaw(T value)
    {
        writeLe(mOut, value);
    }

    void writeRawBytes(const std::uint8_t* data, std::size_t size)
    {
        copyData(mOut, data, size);
    }

private:
    buffer& mOut;
};

// ============================================================================
// Decoder
// ============================================================================

class Decoder
{
public:
    Decoder(const std::uint8_t* data, std::size_t size) noexcept
        : mData(data)
        , mSize(size)
        , mPos(0)
    {
    }

    explicit Decoder(const std::vector<std::uint8_t>& buffer) noexcept
        : Decoder(buffer.data(), buffer.size())
    {
    }

    bool eof() const noexcept
    {
        return mPos >= mSize;
    }

    std::size_t remaining() const noexcept
    {
        return mSize - mPos;
    }

    std::size_t position() const noexcept
    {
        return mPos;
    }

    TypeTag peekType() const
    {
        ensureAvailable(mSize, mPos, 1);
        return static_cast<TypeTag>(mData[mPos]);
    }

    std::uint8_t readUint8()
    {
        expectTag(TypeTag::Uint8);
        return mData[mPos++];
    }

    std::uint16_t readUint16()
    {
        expectTag(TypeTag::Uint16);
        return readLe<std::uint16_t>(mData, mPos, mSize);
    }

    std::uint32_t readUint32()
    {
        expectTag(TypeTag::Uint32);
        return readLe<std::uint32_t>(mData, mPos, mSize);
    }

    std::uint64_t readUint64()
    {
        expectTag(TypeTag::Uint64);
        return readLe<std::uint64_t>(mData, mPos, mSize);
    }

    std::int8_t readInt8()
    {
        expectTag(TypeTag::Int8);
        return static_cast<std::int8_t>(mData[mPos++]);
    }

    std::int16_t readInt16()
    {
        expectTag(TypeTag::Int16);
        return readLe<std::int16_t>(mData, mPos, mSize);
    }

    std::int32_t readInt32()
    {
        expectTag(TypeTag::Int32);
        return readLe<std::int32_t>(mData, mPos, mSize);
    }

    std::int64_t readInt64()
    {
        expectTag(TypeTag::Int64);
        return readLe<std::int64_t>(mData, mPos, mSize);
    }

    float readFloat()
    {
        expectTag(TypeTag::Float32);
        return readLe<float>(mData, mPos, mSize);
    }

    double readDouble()
    {
        expectTag(TypeTag::Float64);
        return readLe<double>(mData, mPos, mSize);
    }

    bool readBool()
    {
        expectTag(TypeTag::Bool);
        ensureAvailable(mSize, mPos, 1);
        return mData[mPos++] != 0;
    }

    std::string readString()
    {
        expectTag(TypeTag::String);
        const auto len64 = readLe<std::uint64_t>(mData, mPos, mSize);
        const auto len = safeToSizeT(len64, "string");
        ensureAvailable(mSize, mPos, len);

        std::string result(reinterpret_cast<const char*>(mData + mPos), len);
        mPos += len;
        return result;
    }

    std::vector<std::uint8_t> readBytes()
    {
        expectTag(TypeTag::Bytes);
        const auto len64 = readLe<std::uint64_t>(mData, mPos, mSize);
        const auto len = safeToSizeT(len64, "bytes");
        ensureAvailable(mSize, mPos, len);

        std::vector<std::uint8_t> result(mData + mPos, mData + mPos + len);
        mPos += len;
        return result;
    }

    std::size_t readArrayLength()
    {
        expectTag(TypeTag::Array);
        const auto len64 = readLe<std::uint64_t>(mData, mPos, mSize);
        return safeToSizeT(len64, "array");
    }

    std::size_t readMapLength()
    {
        expectTag(TypeTag::Map);
        const auto len64 = readLe<std::uint64_t>(mData, mPos, mSize);
        return safeToSizeT(len64, "map");
    }

    // Untagged reads for when caller manages type information
    template <typename T>
    T readRaw()
    {
        return readLe<T>(mData, mPos, mSize);
    }

    void readRawBytes(std::uint8_t* dest, std::size_t len)
    {
        ensureAvailable(mSize, mPos, len);
        std::memcpy(dest, mData + mPos, len);
        mPos += len;
    }

private:
    const std::uint8_t* mData;
    std::size_t mSize;
    std::size_t mPos;

    void expectTag(TypeTag expected)
    {
        ensureAvailable(mSize, mPos, 1);
        const auto actual = static_cast<TypeTag>(mData[mPos++]);
        if (actual != expected)
        {
            throw std::runtime_error("Binary: type mismatch, expected " + std::to_string(static_cast<int>(expected)) +
                                     " got " + std::to_string(static_cast<int>(actual)));
        }
    }
};

// ============================================================================
// Stream-Based Archives (for intrusive serialization)
// ============================================================================

class OutputArchive
{
public:
    using is_loading = std::false_type;

    explicit OutputArchive(std::ostream& os)
        : mOs(os)
    {
        if (!os.good())
        {
            throw std::runtime_error("Binary: output stream not in good state");
        }
    }

    template <typename T>
    OutputArchive& operator&(const T& value)
    {
        serializeImpl(value);
        return *this;
    }

    template <typename T>
    OutputArchive& operator<<(const T& value)
    {
        return operator&(value);
    }

    bool good() const
    {
        return mOs.good();
    }

    void flush()
    {
        mOs.flush();
    }

private:
    std::ostream& mOs;

    template <typename T>
    void serializeImpl(const T& value)
    {
        if constexpr (std::is_arithmetic_v<T> || std::is_enum_v<T>)
        {
            mOs.write(reinterpret_cast<const char*>(&value), sizeof(T));
        }
        else if constexpr (std::is_same_v<T, std::string>)
        {
            const std::size_t len = value.size();
            mOs.write(reinterpret_cast<const char*>(&len), sizeof(len));
            if (len > 0)
            {
                mOs.write(value.data(), static_cast<std::streamsize>(len));
            }
        }
        else
        {
            // Intrusive serialization: call member function
            const_cast<T&>(value).serialize(*this);
        }

        if (!mOs.good())
        {
            throw std::runtime_error("Binary: write failed");
        }
    }
};

class InputArchive
{
public:
    using is_loading = std::true_type;

    explicit InputArchive(std::istream& is)
        : mIs(is)
    {
        if (!is.good())
        {
            throw std::runtime_error("Binary: input stream not in good state");
        }
    }

    template <typename T>
    InputArchive& operator&(T& value)
    {
        deserializeImpl(value);
        return *this;
    }

    template <typename T>
    InputArchive& operator>>(T& value)
    {
        return operator&(value);
    }

    bool good() const
    {
        return mIs.good();
    }

private:
    std::istream& mIs;

    static constexpr std::size_t kMaxStringLength = 16 * 1024 * 1024; // 16MB default

    template <typename T>
    void deserializeImpl(T& value)
    {
        if constexpr (std::is_arithmetic_v<T> || std::is_enum_v<T>)
        {
            mIs.read(reinterpret_cast<char*>(&value), sizeof(T));
        }
        else if constexpr (std::is_same_v<T, std::string>)
        {
            std::size_t len = 0;
            mIs.read(reinterpret_cast<char*>(&len), sizeof(len));

            if (!mIs.good())
            {
                throw std::runtime_error("Binary: failed to read string length");
            }

            if (len > kMaxStringLength)
            {
                throw std::runtime_error("Binary: string length exceeds limit");
            }

            value.resize(len);
            if (len > 0)
            {
                mIs.read(&value[0], static_cast<std::streamsize>(len));
            }
        }
        else
        {
            // Intrusive serialization: call member function
            value.serialize(*this);
        }

        if (!mIs.good())
        {
            throw std::runtime_error("Binary: read failed");
        }
    }
};

} // namespace binary
} // namespace fat_p
