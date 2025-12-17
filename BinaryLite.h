// BinaryLite.h
// Lightweight binary serialization library - standalone with minimal dependencies
// Simple little-endian format optimized for speed and simplicity
// C++17, header-only
//
// This is the STANDALONE version with minimal external dependencies.
// For fat_p component integration (Expected-based API, HpcVector buffers,
// fat_p type serialization), use FatPBinary.h instead.

#ifndef FATP_BINARY_LITE_H
#define FATP_BINARY_LITE_H

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

inline void ensure_available(std::size_t size,
                             std::size_t pos,
                             std::size_t required)
{
    if (pos + required > size)
    {
        throw std::runtime_error("Binary: buffer underflow");
    }
}

inline std::size_t safe_to_size_t(std::uint64_t value, const char* context)
{
    if (value > std::numeric_limits<std::size_t>::max())
    {
        throw std::runtime_error(std::string("Binary: ") + context +
                                 " length exceeds platform limits");
    }
    return static_cast<std::size_t>(value);
}

template <typename T>
void write_le(std::vector<std::uint8_t>& buffer, T value)
{
    static_assert(std::is_trivially_copyable_v<T>,
                  "Type must be trivially copyable");

    const auto* bytes = reinterpret_cast<const std::uint8_t*>(&value);
    buffer.insert(buffer.end(), bytes, bytes + sizeof(T));
}

template <typename T>
T read_le(const std::uint8_t* data, std::size_t& pos, std::size_t size)
{
    static_assert(std::is_trivially_copyable_v<T>,
                  "Type must be trivially copyable");

    ensure_available(size, pos, sizeof(T));

    T value;
    std::memcpy(&value, data + pos, sizeof(T));
    pos += sizeof(T);
    return value;
}

inline void copy_data(std::vector<std::uint8_t>& buffer,
                      const std::uint8_t* src,
                      std::size_t len)
{
#ifdef __AVX2__
    const std::size_t old_size = buffer.size();
    buffer.resize(old_size + len);
    std::uint8_t* dst = buffer.data() + old_size;

    std::size_t i = 0;
    for (; i + 32 <= len; i += 32)
    {
        __m256i chunk = _mm256_loadu_si256(
            reinterpret_cast<const __m256i*>(src + i));
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
        : out_(out)
    {
    }

    buffer& output() noexcept
    {
        return out_;
    }

    void write_uint8(std::uint8_t value)
    {
        out_.push_back(static_cast<std::uint8_t>(TypeTag::Uint8));
        out_.push_back(value);
    }

    void write_uint16(std::uint16_t value)
    {
        out_.push_back(static_cast<std::uint8_t>(TypeTag::Uint16));
        write_le(out_, value);
    }

    void write_uint32(std::uint32_t value)
    {
        out_.push_back(static_cast<std::uint8_t>(TypeTag::Uint32));
        write_le(out_, value);
    }

    void write_uint64(std::uint64_t value)
    {
        out_.push_back(static_cast<std::uint8_t>(TypeTag::Uint64));
        write_le(out_, value);
    }

    void write_int8(std::int8_t value)
    {
        out_.push_back(static_cast<std::uint8_t>(TypeTag::Int8));
        out_.push_back(static_cast<std::uint8_t>(value));
    }

    void write_int16(std::int16_t value)
    {
        out_.push_back(static_cast<std::uint8_t>(TypeTag::Int16));
        write_le(out_, value);
    }

    void write_int32(std::int32_t value)
    {
        out_.push_back(static_cast<std::uint8_t>(TypeTag::Int32));
        write_le(out_, value);
    }

    void write_int64(std::int64_t value)
    {
        out_.push_back(static_cast<std::uint8_t>(TypeTag::Int64));
        write_le(out_, value);
    }

    void write_float(float value)
    {
        out_.push_back(static_cast<std::uint8_t>(TypeTag::Float32));
        write_le(out_, value);
    }

    void write_double(double value)
    {
        out_.push_back(static_cast<std::uint8_t>(TypeTag::Float64));
        write_le(out_, value);
    }

    void write_bool(bool value)
    {
        out_.push_back(static_cast<std::uint8_t>(TypeTag::Bool));
        out_.push_back(value ? 1U : 0U);
    }

    void write_string(const std::string& value)
    {
        out_.push_back(static_cast<std::uint8_t>(TypeTag::String));
        write_le(out_, static_cast<std::uint64_t>(value.size()));
        copy_data(out_,
                  reinterpret_cast<const std::uint8_t*>(value.data()),
                  value.size());
    }

    void write_bytes(const std::uint8_t* data, std::size_t size)
    {
        out_.push_back(static_cast<std::uint8_t>(TypeTag::Bytes));
        write_le(out_, static_cast<std::uint64_t>(size));
        copy_data(out_, data, size);
    }

    void write_bytes(const std::vector<std::uint8_t>& data)
    {
        write_bytes(data.data(), data.size());
    }

    void begin_array(std::size_t size)
    {
        out_.push_back(static_cast<std::uint8_t>(TypeTag::Array));
        write_le(out_, static_cast<std::uint64_t>(size));
    }

    void begin_map(std::size_t size)
    {
        out_.push_back(static_cast<std::uint8_t>(TypeTag::Map));
        write_le(out_, static_cast<std::uint64_t>(size));
    }

    // Untagged writes for when caller manages type information
    template <typename T>
    void write_raw(T value)
    {
        write_le(out_, value);
    }

    void write_raw_bytes(const std::uint8_t* data, std::size_t size)
    {
        copy_data(out_, data, size);
    }

private:
    buffer& out_;
};

// ============================================================================
// Decoder
// ============================================================================

class Decoder
{
public:
    Decoder(const std::uint8_t* data, std::size_t size) noexcept
        : data_(data)
        , size_(size)
        , pos_(0)
    {
    }

    explicit Decoder(const std::vector<std::uint8_t>& buffer) noexcept
        : Decoder(buffer.data(), buffer.size())
    {
    }

    bool eof() const noexcept
    {
        return pos_ >= size_;
    }

    std::size_t remaining() const noexcept
    {
        return size_ - pos_;
    }

    std::size_t position() const noexcept
    {
        return pos_;
    }

    TypeTag peek_type() const
    {
        ensure_available(size_, pos_, 1);
        return static_cast<TypeTag>(data_[pos_]);
    }

    std::uint8_t read_uint8()
    {
        expect_tag(TypeTag::Uint8);
        return data_[pos_++];
    }

    std::uint16_t read_uint16()
    {
        expect_tag(TypeTag::Uint16);
        return read_le<std::uint16_t>(data_, pos_, size_);
    }

    std::uint32_t read_uint32()
    {
        expect_tag(TypeTag::Uint32);
        return read_le<std::uint32_t>(data_, pos_, size_);
    }

    std::uint64_t read_uint64()
    {
        expect_tag(TypeTag::Uint64);
        return read_le<std::uint64_t>(data_, pos_, size_);
    }

    std::int8_t read_int8()
    {
        expect_tag(TypeTag::Int8);
        return static_cast<std::int8_t>(data_[pos_++]);
    }

    std::int16_t read_int16()
    {
        expect_tag(TypeTag::Int16);
        return read_le<std::int16_t>(data_, pos_, size_);
    }

    std::int32_t read_int32()
    {
        expect_tag(TypeTag::Int32);
        return read_le<std::int32_t>(data_, pos_, size_);
    }

    std::int64_t read_int64()
    {
        expect_tag(TypeTag::Int64);
        return read_le<std::int64_t>(data_, pos_, size_);
    }

    float read_float()
    {
        expect_tag(TypeTag::Float32);
        return read_le<float>(data_, pos_, size_);
    }

    double read_double()
    {
        expect_tag(TypeTag::Float64);
        return read_le<double>(data_, pos_, size_);
    }

    bool read_bool()
    {
        expect_tag(TypeTag::Bool);
        ensure_available(size_, pos_, 1);
        return data_[pos_++] != 0;
    }

    std::string read_string()
    {
        expect_tag(TypeTag::String);
        const auto len64 = read_le<std::uint64_t>(data_, pos_, size_);
        const auto len = safe_to_size_t(len64, "string");
        ensure_available(size_, pos_, len);

        std::string result(reinterpret_cast<const char*>(data_ + pos_), len);
        pos_ += len;
        return result;
    }

    std::vector<std::uint8_t> read_bytes()
    {
        expect_tag(TypeTag::Bytes);
        const auto len64 = read_le<std::uint64_t>(data_, pos_, size_);
        const auto len = safe_to_size_t(len64, "bytes");
        ensure_available(size_, pos_, len);

        std::vector<std::uint8_t> result(data_ + pos_, data_ + pos_ + len);
        pos_ += len;
        return result;
    }

    std::size_t read_array_length()
    {
        expect_tag(TypeTag::Array);
        const auto len64 = read_le<std::uint64_t>(data_, pos_, size_);
        return safe_to_size_t(len64, "array");
    }

    std::size_t read_map_length()
    {
        expect_tag(TypeTag::Map);
        const auto len64 = read_le<std::uint64_t>(data_, pos_, size_);
        return safe_to_size_t(len64, "map");
    }

    // Untagged reads for when caller manages type information
    template <typename T>
    T read_raw()
    {
        return read_le<T>(data_, pos_, size_);
    }

    void read_raw_bytes(std::uint8_t* dest, std::size_t len)
    {
        ensure_available(size_, pos_, len);
        std::memcpy(dest, data_ + pos_, len);
        pos_ += len;
    }

private:
    const std::uint8_t* data_;
    std::size_t size_;
    std::size_t pos_;

    void expect_tag(TypeTag expected)
    {
        ensure_available(size_, pos_, 1);
        const auto actual = static_cast<TypeTag>(data_[pos_++]);
        if (actual != expected)
        {
            throw std::runtime_error(
                "Binary: type mismatch, expected " +
                std::to_string(static_cast<int>(expected)) +
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
        : os_(os)
    {
        if (!os.good())
        {
            throw std::runtime_error("Binary: output stream not in good state");
        }
    }

    template <typename T>
    OutputArchive& operator&(const T& value)
    {
        serialize_impl(value);
        return *this;
    }

    template <typename T>
    OutputArchive& operator<<(const T& value)
    {
        return operator&(value);
    }

    bool good() const
    {
        return os_.good();
    }

    void flush()
    {
        os_.flush();
    }

private:
    std::ostream& os_;

    template <typename T>
    void serialize_impl(const T& value)
    {
        if constexpr (std::is_arithmetic_v<T> || std::is_enum_v<T>)
        {
            os_.write(reinterpret_cast<const char*>(&value), sizeof(T));
        }
        else if constexpr (std::is_same_v<T, std::string>)
        {
            const std::size_t len = value.size();
            os_.write(reinterpret_cast<const char*>(&len), sizeof(len));
            if (len > 0)
            {
                os_.write(value.data(), static_cast<std::streamsize>(len));
            }
        }
        else
        {
            // Intrusive serialization: call member function
            const_cast<T&>(value).serialize(*this);
        }

        if (!os_.good())
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
        : is_(is)
    {
        if (!is.good())
        {
            throw std::runtime_error("Binary: input stream not in good state");
        }
    }

    template <typename T>
    InputArchive& operator&(T& value)
    {
        deserialize_impl(value);
        return *this;
    }

    template <typename T>
    InputArchive& operator>>(T& value)
    {
        return operator&(value);
    }

    bool good() const
    {
        return is_.good();
    }

private:
    std::istream& is_;

    static constexpr std::size_t MAX_STRING_LENGTH = 16 * 1024 * 1024;  // 16MB default

    template <typename T>
    void deserialize_impl(T& value)
    {
        if constexpr (std::is_arithmetic_v<T> || std::is_enum_v<T>)
        {
            is_.read(reinterpret_cast<char*>(&value), sizeof(T));
        }
        else if constexpr (std::is_same_v<T, std::string>)
        {
            std::size_t len = 0;
            is_.read(reinterpret_cast<char*>(&len), sizeof(len));

            if (!is_.good())
            {
                throw std::runtime_error("Binary: failed to read string length");
            }

            if (len > MAX_STRING_LENGTH)
            {
                throw std::runtime_error("Binary: string length exceeds limit");
            }

            value.resize(len);
            if (len > 0)
            {
                is_.read(&value[0], static_cast<std::streamsize>(len));
            }
        }
        else
        {
            // Intrusive serialization: call member function
            value.serialize(*this);
        }

        if (!is_.good())
        {
            throw std::runtime_error("Binary: read failed");
        }
    }
};

} // namespace binary
} // namespace fat_p

#endif // FATP_BINARY_LITE_H
