#pragma once
/*
FATP_META:
  meta_version: 1
  component: Skeleton
  file_role: public_header
  path: include/fat_p/SkeletonSerializer.h
  namespace: fat_p::skeleton
  layer: Domain
  summary: >
    SerializeWriter and SerializeReader: thin wrappers over std::ostream and
    std::istream for item-level binary serialization. Little-endian, no type
    tags — caller-managed schema. Matches BE's STORE_POD/LOAD_POD pattern
    adapted to modern C++.
  api_stability: in_work
  related:
    docs_search: "Skeleton serialization"
    tests:
      - components/Skeleton/tests/test_SkeletonSerializer.cpp
  hygiene:
    pragma_once: true
    include_guard: false
    defines_total: 0
    defines_unprefixed: 0
    undefs_total: 0
    includes_windows_h: false
*/

// SerializeWriter and SerializeReader are the fat_p-layer serialization
// primitives. They wrap a raw std::ostream / std::istream and provide
// fixed-width little-endian read/write methods with no type-tag overhead.
//
// Design choices (grounded in BE):
//
// 1. Stream-based, not buffer-based. BE used CArchive (which wraps CFile)
//    and raw std::ostream/std::istream for its BinaryIO methods. File I/O
//    maps directly to stream construction. BinaryLite's Encoder/Decoder
//    use vector<uint8_t> buffers — useful for in-memory work but an extra
//    copy for file paths. These wrappers bridge the two: stream for the
//    transport, fixed-width for the wire format.
//
// 2. No type tags. BE's STORE_POD/LOAD_POD wrote raw bytes with no
//    self-describing envelope. The save format is an implicit schema:
//    the writer and reader must agree on field order and types. This is
//    correct for versioned item serialization where the version number
//    (written by the save orchestrator) selects the reader path.
//
// 3. Little-endian wire format. Matches BinaryLite convention and the
//    dominant platform (x86-64). On big-endian hosts the byte-swap path
//    activates via std::endian.
//
// 4. BoneId has dedicated writeBoneId/readBoneId methods that delegate to
//    BoneId::serialize/deserialize (1 depth byte + 2 bytes per active level,
//    canonical form). readBoneIdLegacy9 reads the pre-widening 9-byte form
//    for version-gated migration branches. Application code should not
//    decompose BoneId into levels manually.
//
// 5. String length is stored as uint32_t (not uint64_t). No Loom string
//    field will exceed 4 GiB. This halves the length prefix cost compared
//    to BinaryLite's uint64_t length prefix.
//
// Error model: read methods throw std::runtime_error on stream failure or
// truncation. Write methods propagate stream exceptions if the stream has
// exceptions() enabled, but do not throw independently.
//
// Forward declarations of SerializeWriter and SerializeReader live in
// Skeleton.h (Phase 0) so that the SkeletonItem default no-op
// serialize/deserialize bodies compile without this header.

/**
 * @file SkeletonSerializer.h
 * @brief Binary stream wrappers for skeleton item serialization.
 */

#include <bit>
#include <cstdint>
#include <cstring>
#include <istream>
#include <ostream>
#include <stdexcept>
#include <string>

#include "SkeletonFwd.h"

namespace fat_p::skeleton
{

// =============================================================================
// Byte-order helpers (internal)
// =============================================================================

namespace detail
{

inline constexpr bool kLittleEndian = (std::endian::native == std::endian::little);

// Store a trivially-copyable value in little-endian form.
template <typename T>
void writeLe(std::ostream& out, T value) noexcept(false)
{
    static_assert(std::is_trivially_copyable_v<T>);

    if constexpr (!kLittleEndian && sizeof(T) > 1)
    {
        // Byte-swap on big-endian hosts.
        auto* p = reinterpret_cast<std::uint8_t*>(&value);
        for (std::size_t i = 0; i < sizeof(T) / 2; ++i)
        {
            std::swap(p[i], p[sizeof(T) - 1 - i]);
        }
    }

    out.write(reinterpret_cast<const char*>(&value), sizeof(T));
}

// Read a trivially-copyable value from little-endian form.
template <typename T>
T readLe(std::istream& in)
{
    static_assert(std::is_trivially_copyable_v<T>);

    T value{};
    in.read(reinterpret_cast<char*>(&value), sizeof(T));

    if (!in.good())
    {
        throw std::runtime_error("SkeletonSerializer: unexpected end of stream");
    }

    if constexpr (!kLittleEndian && sizeof(T) > 1)
    {
        auto* p = reinterpret_cast<std::uint8_t*>(&value);
        for (std::size_t i = 0; i < sizeof(T) / 2; ++i)
        {
            std::swap(p[i], p[sizeof(T) - 1 - i]);
        }
    }

    return value;
}

} // namespace detail

// =============================================================================
// SerializeWriter
// =============================================================================

/**
 * @brief Writes fixed-width little-endian values to an output stream.
 *
 * Does not own the stream. Caller manages stream lifetime and flush/close.
 *
 * @note Not thread-safe. A single writer should not be shared across threads.
 */
class SerializeWriter
{
public:
    /// @brief Constructs a writer over @p out.
    explicit SerializeWriter(std::ostream& out) noexcept
        : mOut(out)
    {
    }

    // -- Integer writes -------------------------------------------------------

    void writeU8(std::uint8_t v)   { detail::writeLe(mOut, v); }
    void writeU16(std::uint16_t v) { detail::writeLe(mOut, v); }
    void writeU32(std::uint32_t v) { detail::writeLe(mOut, v); }
    void writeU64(std::uint64_t v) { detail::writeLe(mOut, v); }

    void writeI8(std::int8_t v)    { detail::writeLe(mOut, v); }
    void writeI16(std::int16_t v)  { detail::writeLe(mOut, v); }
    void writeI32(std::int32_t v)  { detail::writeLe(mOut, v); }
    void writeI64(std::int64_t v)  { detail::writeLe(mOut, v); }

    // -- Floating point -------------------------------------------------------

    void writeFloat(float v)   { detail::writeLe(mOut, v); }
    void writeDouble(double v) { detail::writeLe(mOut, v); }

    // -- Bool -----------------------------------------------------------------

    // Stored as a single byte: 0x01 for true, 0x00 for false.
    // BE used a 4/8-byte pattern (TRUE_PATTERN/FALSE_PATTERN) for corruption
    // detection. We trade that for compactness; the version-checked schema and
    // item count header provide sufficient integrity.
    void writeBool(bool v)
    {
        const std::uint8_t byte = v ? 1u : 0u;
        detail::writeLe(mOut, byte);
    }

    // -- String ---------------------------------------------------------------

    // Length prefix is uint32_t (max ~4 GiB per string).
    void writeString(std::string_view v)
    {
        const auto len = static_cast<std::uint32_t>(v.size());
        detail::writeLe(mOut, len);
        mOut.write(v.data(), static_cast<std::streamsize>(len));
    }

    // -- Raw bytes ------------------------------------------------------------

    void writeBytes(const void* data, std::size_t size)
    {
        const auto len = static_cast<std::uint32_t>(size);
        detail::writeLe(mOut, len);
        mOut.write(static_cast<const char*>(data), static_cast<std::streamsize>(len));
    }

    // -- BoneId ---------------------------------------------------------------

    // Delegates to BoneId::serialize() — 1 depth byte + 2 bytes per active
    // level, canonical. Variable size: 1..BoneId::kMaxSerializedBytes.
    void writeBoneId(BoneId id)
    {
        std::byte buf[BoneId::kMaxSerializedBytes];
        const std::size_t used =
            id.serialize(std::span<std::byte, BoneId::kMaxSerializedBytes>{buf});
        mOut.write(reinterpret_cast<const char*>(buf),
                   static_cast<std::streamsize>(used));
    }

    // -- Stream access --------------------------------------------------------

    /// @brief Returns the underlying stream.
    [[nodiscard]] std::ostream& stream() noexcept { return mOut; }

private:
    std::ostream& mOut;
};

// =============================================================================
// SerializeReader
// =============================================================================

/**
 * @brief Reads fixed-width little-endian values from an input stream.
 *
 * Does not own the stream. Read methods throw std::runtime_error on
 * truncation or stream failure.
 *
 * @note Not thread-safe. A single reader should not be shared across threads.
 */
class SerializeReader
{
public:
    /// @brief Constructs a reader over @p in.
    explicit SerializeReader(std::istream& in) noexcept
        : mIn(in)
    {
    }

    // -- Integer reads --------------------------------------------------------

    [[nodiscard]] std::uint8_t  readU8()  { return detail::readLe<std::uint8_t>(mIn); }
    [[nodiscard]] std::uint16_t readU16() { return detail::readLe<std::uint16_t>(mIn); }
    [[nodiscard]] std::uint32_t readU32() { return detail::readLe<std::uint32_t>(mIn); }
    [[nodiscard]] std::uint64_t readU64() { return detail::readLe<std::uint64_t>(mIn); }

    [[nodiscard]] std::int8_t  readI8()  { return detail::readLe<std::int8_t>(mIn); }
    [[nodiscard]] std::int16_t readI16() { return detail::readLe<std::int16_t>(mIn); }
    [[nodiscard]] std::int32_t readI32() { return detail::readLe<std::int32_t>(mIn); }
    [[nodiscard]] std::int64_t readI64() { return detail::readLe<std::int64_t>(mIn); }

    // -- Floating point -------------------------------------------------------

    [[nodiscard]] float  readFloat()  { return detail::readLe<float>(mIn); }
    [[nodiscard]] double readDouble() { return detail::readLe<double>(mIn); }

    // -- Bool -----------------------------------------------------------------

    [[nodiscard]] bool readBool()
    {
        return detail::readLe<std::uint8_t>(mIn) != 0u;
    }

    // -- String ---------------------------------------------------------------

    [[nodiscard]] std::string readString()
    {
        const auto len = detail::readLe<std::uint32_t>(mIn);

        std::string result(len, '\0');
        mIn.read(result.data(), static_cast<std::streamsize>(len));

        if (!mIn.good())
        {
            throw std::runtime_error("SkeletonSerializer: truncated string read");
        }

        return result;
    }

    // -- Raw bytes ------------------------------------------------------------

    // Returns a length-prefixed byte blob.
    [[nodiscard]] std::vector<std::uint8_t> readBytes()
    {
        const auto len = detail::readLe<std::uint32_t>(mIn);

        std::vector<std::uint8_t> result(len);
        mIn.read(reinterpret_cast<char*>(result.data()),
                 static_cast<std::streamsize>(len));

        if (!mIn.good())
        {
            throw std::runtime_error("SkeletonSerializer: truncated bytes read");
        }

        return result;
    }

    // -- BoneId ---------------------------------------------------------------

    // Reads the current format: 1 depth byte + 2 bytes per active level.
    [[nodiscard]] BoneId readBoneId()
    {
        std::byte buf[BoneId::kMaxSerializedBytes];
        mIn.read(reinterpret_cast<char*>(buf), 1);
        if (!mIn.good())
        {
            throw std::runtime_error("SkeletonSerializer: truncated BoneId read");
        }
        const auto depth = std::to_integer<std::uint8_t>(buf[0]);
        if (depth > BoneId::kMaxDepth)
        {
            throw std::runtime_error("SkeletonSerializer: corrupt BoneId depth");
        }
        const std::streamsize levelBytes = 2 * static_cast<std::streamsize>(depth);
        mIn.read(reinterpret_cast<char*>(buf + 1), levelBytes);
        if (!mIn.good())
        {
            throw std::runtime_error("SkeletonSerializer: truncated BoneId read");
        }
        return BoneId::deserialize(
            std::span<const std::byte>{buf, 1u + 2u * static_cast<std::size_t>(depth)});
    }

    // Reads the legacy 9-byte form (8 x 8-bit levels + depth) written before
    // the 16x16 widening. Callers select this in version-gated deserialize
    // branches for payloads saved by older builds.
    [[nodiscard]] BoneId readBoneIdLegacy9()
    {
        std::byte buf[BoneId::kLegacySerializedBytes];
        mIn.read(reinterpret_cast<char*>(buf), BoneId::kLegacySerializedBytes);

        if (!mIn.good())
        {
            throw std::runtime_error("SkeletonSerializer: truncated BoneId read");
        }

        return BoneId::deserializeLegacy9(
            std::span<const std::byte, BoneId::kLegacySerializedBytes>{buf});
    }

    // -- Stream queries -------------------------------------------------------

    /// @brief Returns true if no more data is available.
    [[nodiscard]] bool atEnd()
    {
        return mIn.peek() == std::istream::traits_type::eof();
    }

    /// @brief Returns the underlying stream.
    [[nodiscard]] std::istream& stream() noexcept { return mIn; }

private:
    std::istream& mIn;
};

} // namespace fat_p::skeleton
