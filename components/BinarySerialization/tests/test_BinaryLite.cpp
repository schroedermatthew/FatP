/**
 * @file test_BinaryLite.cpp
 * @brief Comprehensive unit tests for BinaryLite.h
 */
/*
FATP_META:
  meta_version: 1
  component: BinarySerialization
  file_role: test
  path: components/BinarySerialization/tests/test_BinaryLite.cpp
  layer: Testing
  namespace: fat_p
  summary: "Unit tests for BinaryLite."
  api_stability: in_work
  related:
    docs_search: "BinaryLite"
    headers:
      - include/fat_p/BinaryLite.h
      - include/fat_p/FatPTest.h
  hygiene:
    pragma_once: false
    include_guard: false
    defines_total: 0
    defines_unprefixed: 0
    undefs_total: 0
    includes_windows_h: false
  generated:
    by: fatp-meta-tool
    mode: autogen
*/

#include <cstdint>
#include <limits>
#include <random>
#include <string>
#include <vector>

#include "BinaryLite.h"
#include "FatPTest.h"

namespace fat_p::testing::binarylite
{

using fat_p::binary::Decoder;
using fat_p::binary::Encoder;
using fat_p::binary::TypeTag;

// ============================================================================
// Primitive Roundtrips
// ============================================================================

FATP_TEST_CASE(uint8_roundtrip)
{
    const std::uint8_t values[] = {0U, 1U, 127U, 255U};

    for (std::uint8_t v : values)
    {
        std::vector<std::uint8_t> buf;
        Encoder enc(buf);
        enc.writeUint8(v);

        Decoder dec(buf);
        FATP_ASSERT_EQ(dec.readUint8(), v, "uint8 roundtrip");
    }

    return true;
}

FATP_TEST_CASE(uint16_roundtrip)
{
    const std::uint16_t values[] = {0U, 1U, 255U, 256U, 65535U};

    for (std::uint16_t v : values)
    {
        std::vector<std::uint8_t> buf;
        Encoder enc(buf);
        enc.writeUint16(v);

        Decoder dec(buf);
        FATP_ASSERT_EQ(dec.readUint16(), v, "uint16 roundtrip");
    }

    return true;
}

FATP_TEST_CASE(uint32_roundtrip)
{
    const std::uint32_t values[] = {0U, 1U, 65535U, 65536U, 0xDEADBEEFU};

    for (std::uint32_t v : values)
    {
        std::vector<std::uint8_t> buf;
        Encoder enc(buf);
        enc.writeUint32(v);

        Decoder dec(buf);
        FATP_ASSERT_EQ(dec.readUint32(), v, "uint32 roundtrip");
    }

    return true;
}

FATP_TEST_CASE(uint64_roundtrip)
{
    const std::uint64_t values[] =
        {0ULL, 1ULL, 255ULL, 256ULL, 65535ULL, 0xDEADBEEFCAFEBABEULL, std::numeric_limits<std::uint64_t>::max()};

    for (std::uint64_t v : values)
    {
        std::vector<std::uint8_t> buf;
        Encoder enc(buf);
        enc.writeUint64(v);

        Decoder dec(buf);
        FATP_ASSERT_EQ(dec.readUint64(), v, "uint64 roundtrip");
    }

    return true;
}

FATP_TEST_CASE(int8_roundtrip)
{
    const std::int8_t values[] = {0, 1, -1, 127, -128};

    for (std::int8_t v : values)
    {
        std::vector<std::uint8_t> buf;
        Encoder enc(buf);
        enc.writeInt8(v);

        Decoder dec(buf);
        FATP_ASSERT_EQ(dec.readInt8(), v, "int8 roundtrip");
    }

    return true;
}

FATP_TEST_CASE(int16_roundtrip)
{
    const std::int16_t values[] = {0, 1, -1, 32767, -32768};

    for (std::int16_t v : values)
    {
        std::vector<std::uint8_t> buf;
        Encoder enc(buf);
        enc.writeInt16(v);

        Decoder dec(buf);
        FATP_ASSERT_EQ(dec.readInt16(), v, "int16 roundtrip");
    }

    return true;
}

FATP_TEST_CASE(int32_roundtrip)
{
    const std::int32_t values[] = {0,
                                   1,
                                   -1,
                                   1234567,
                                   -9876543,
                                   std::numeric_limits<std::int32_t>::min(),
                                   std::numeric_limits<std::int32_t>::max()};

    for (std::int32_t v : values)
    {
        std::vector<std::uint8_t> buf;
        Encoder enc(buf);
        enc.writeInt32(v);

        Decoder dec(buf);
        FATP_ASSERT_EQ(dec.readInt32(), v, "int32 roundtrip");
    }

    return true;
}

FATP_TEST_CASE(int64_roundtrip)
{
    const std::int64_t values[] = {0,
                                   1,
                                   -1,
                                   1234567,
                                   -9876543,
                                   std::numeric_limits<std::int64_t>::min(),
                                   std::numeric_limits<std::int64_t>::max()};

    for (std::int64_t v : values)
    {
        std::vector<std::uint8_t> buf;
        Encoder enc(buf);
        enc.writeInt64(v);

        Decoder dec(buf);
        FATP_ASSERT_EQ(dec.readInt64(), v, "int64 roundtrip");
    }

    return true;
}

FATP_TEST_CASE(bool_roundtrip)
{
    std::vector<std::uint8_t> buf;
    Encoder enc(buf);
    enc.writeBool(false);
    enc.writeBool(true);

    Decoder dec(buf);
    FATP_ASSERT_TRUE(!dec.readBool(), "false roundtrip");
    FATP_ASSERT_TRUE(dec.readBool(), "true roundtrip");

    return true;
}

FATP_TEST_CASE(float_roundtrip)
{
    const float values[] = {0.0f, -0.0f, 1.5f, -2.75f, 3.14159f};

    for (float v : values)
    {
        std::vector<std::uint8_t> buf;
        Encoder enc(buf);
        enc.writeFloat(v);

        Decoder dec(buf);
        FATP_ASSERT_EQ(dec.readFloat(), v, "float roundtrip");
    }

    return true;
}

FATP_TEST_CASE(double_roundtrip)
{
    const double values[] = {0.0, -0.0, 1.5, -2.75, 1e10, -3.14159265358979};

    for (double v : values)
    {
        std::vector<std::uint8_t> buf;
        Encoder enc(buf);
        enc.writeDouble(v);

        Decoder dec(buf);
        FATP_ASSERT_EQ(dec.readDouble(), v, "double roundtrip");
    }

    return true;
}

FATP_TEST_CASE(string_roundtrip)
{
    const std::string values[] = {"", "a", "hello", "UTF-8 \xC3\xA9\xC3\xA0"};

    for (const auto& s : values)
    {
        std::vector<std::uint8_t> buf;
        Encoder enc(buf);
        enc.writeString(s);

        Decoder dec(buf);
        FATP_ASSERT_EQ(dec.readString(), s, "string roundtrip");
    }

    return true;
}

FATP_TEST_CASE(bytes_roundtrip)
{
    const std::vector<std::uint8_t> payload = {1U, 2U, 3U, 4U, 5U, 0U, 255U};

    std::vector<std::uint8_t> buf;
    Encoder enc(buf);
    enc.writeBytes(payload);

    Decoder dec(buf);
    const auto result = dec.readBytes();
    FATP_ASSERT_EQ(result.size(), payload.size(), "bytes size");
    for (std::size_t i = 0; i < payload.size(); ++i)
    {
        FATP_ASSERT_EQ(result[i], payload[i], "bytes element");
    }

    return true;
}

FATP_TEST_CASE(array_header)
{
    std::vector<std::uint8_t> buf;
    Encoder enc(buf);
    enc.beginArray(5U);

    Decoder dec(buf);
    FATP_ASSERT_EQ(dec.readArrayLength(), 5U, "array length");

    return true;
}

FATP_TEST_CASE(map_header)
{
    std::vector<std::uint8_t> buf;
    Encoder enc(buf);
    enc.beginMap(3U);

    Decoder dec(buf);
    FATP_ASSERT_EQ(dec.readMapLength(), 3U, "map length");

    return true;
}

FATP_TEST_CASE(peek_type)
{
    std::vector<std::uint8_t> buf;
    Encoder enc(buf);
    enc.writeUint32(42U);
    enc.writeString("hello");
    enc.writeBool(true);

    Decoder dec(buf);

    FATP_ASSERT_TRUE(dec.peekType() == TypeTag::Uint32, "peek uint32");
    (void)dec.readUint32();

    FATP_ASSERT_TRUE(dec.peekType() == TypeTag::String, "peek string");
    (void)dec.readString();

    FATP_ASSERT_TRUE(dec.peekType() == TypeTag::Bool, "peek bool");
    (void)dec.readBool();

    FATP_ASSERT_TRUE(dec.eof(), "should be at EOF");

    return true;
}

FATP_TEST_CASE(multiple_values)
{
    std::vector<std::uint8_t> buf;
    Encoder enc(buf);

    enc.writeUint32(42U);
    enc.writeInt64(-123456789LL);
    enc.writeString("test");
    enc.writeDouble(3.14159);
    enc.writeBool(true);

    Decoder dec(buf);

    FATP_ASSERT_EQ(dec.readUint32(), 42U, "uint32");
    FATP_ASSERT_EQ(dec.readInt64(), -123456789LL, "int64");
    FATP_ASSERT_EQ(dec.readString(), "test", "string");
    FATP_ASSERT_EQ(dec.readDouble(), 3.14159, "double");
    FATP_ASSERT_TRUE(dec.readBool(), "bool");
    FATP_ASSERT_TRUE(dec.eof(), "should be at EOF");

    return true;
}

// ============================================================================
// Malformed Input Tests
// ============================================================================

FATP_TEST_CASE(truncated_uint64)
{
    std::vector<std::uint8_t> buf;
    Encoder enc(buf);
    enc.writeUint64(0xDEADBEEFCAFEBABEULL);

    FATP_ASSERT_EQ(buf.size(), 9U, "uint64 should be 1 tag + 8 bytes");

    for (std::size_t cut = 1; cut < buf.size(); ++cut)
    {
        std::vector<std::uint8_t> truncated(buf.begin(), buf.begin() + cut);
        FATP_ASSERT_THROWS(([&] {
                               Decoder dec(truncated);
                               (void)dec.readUint64();
                           }()),
                           std::runtime_error,
                           "truncated uint64 should throw");
    }

    return true;
}

FATP_TEST_CASE(truncated_uint32)
{
    std::vector<std::uint8_t> buf;
    Encoder enc(buf);
    enc.writeUint32(0xDEADBEEFU);

    FATP_ASSERT_EQ(buf.size(), 5U, "uint32 should be 1 tag + 4 bytes");

    for (std::size_t cut = 1; cut < buf.size(); ++cut)
    {
        std::vector<std::uint8_t> truncated(buf.begin(), buf.begin() + cut);
        FATP_ASSERT_THROWS(([&] {
                               Decoder dec(truncated);
                               (void)dec.readUint32();
                           }()),
                           std::runtime_error,
                           "truncated uint32 should throw");
    }

    return true;
}

FATP_TEST_CASE(truncated_string)
{
    std::vector<std::uint8_t> buf;
    Encoder enc(buf);
    enc.writeString("Hello, World!");

    for (std::size_t cut = 1; cut < buf.size(); ++cut)
    {
        std::vector<std::uint8_t> truncated(buf.begin(), buf.begin() + cut);
        FATP_ASSERT_THROWS(([&] {
                               Decoder dec(truncated);
                               (void)dec.readString();
                           }()),
                           std::runtime_error,
                           "truncated string should throw");
    }

    return true;
}

FATP_TEST_CASE(truncated_bytes)
{
    const std::vector<std::uint8_t> payload = {1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U};

    std::vector<std::uint8_t> buf;
    Encoder enc(buf);
    enc.writeBytes(payload);

    for (std::size_t cut = 1; cut < buf.size(); ++cut)
    {
        std::vector<std::uint8_t> truncated(buf.begin(), buf.begin() + cut);
        FATP_ASSERT_THROWS(([&] {
                               Decoder dec(truncated);
                               (void)dec.readBytes();
                           }()),
                           std::runtime_error,
                           "truncated bytes should throw");
    }

    return true;
}

FATP_TEST_CASE(truncated_double)
{
    std::vector<std::uint8_t> buf;
    Encoder enc(buf);
    enc.writeDouble(3.14159265358979);

    FATP_ASSERT_EQ(buf.size(), 9U, "double should be 1 tag + 8 bytes");

    for (std::size_t cut = 1; cut < buf.size(); ++cut)
    {
        std::vector<std::uint8_t> truncated(buf.begin(), buf.begin() + cut);
        FATP_ASSERT_THROWS(([&] {
                               Decoder dec(truncated);
                               (void)dec.readDouble();
                           }()),
                           std::runtime_error,
                           "truncated double should throw");
    }

    return true;
}

FATP_TEST_CASE(wrong_type_uint_from_string)
{
    std::vector<std::uint8_t> buf;
    Encoder enc(buf);
    enc.writeString("not a number");

    FATP_ASSERT_THROWS(([&] {
                           Decoder dec(buf);
                           (void)dec.readUint64();
                       }()),
                       std::runtime_error,
                       "uint64 from string should throw");

    return true;
}

FATP_TEST_CASE(wrong_type_string_from_uint)
{
    std::vector<std::uint8_t> buf;
    Encoder enc(buf);
    enc.writeUint64(12345ULL);

    FATP_ASSERT_THROWS(([&] {
                           Decoder dec(buf);
                           (void)dec.readString();
                       }()),
                       std::runtime_error,
                       "string from uint should throw");

    return true;
}

FATP_TEST_CASE(wrong_type_bool_from_double)
{
    std::vector<std::uint8_t> buf;
    Encoder enc(buf);
    enc.writeDouble(1.0);

    FATP_ASSERT_THROWS(([&] {
                           Decoder dec(buf);
                           (void)dec.readBool();
                       }()),
                       std::runtime_error,
                       "bool from double should throw");

    return true;
}

FATP_TEST_CASE(wrong_type_int32_from_int64)
{
    std::vector<std::uint8_t> buf;
    Encoder enc(buf);
    enc.writeInt64(12345LL);

    FATP_ASSERT_THROWS(([&] {
                           Decoder dec(buf);
                           (void)dec.readInt32();
                       }()),
                       std::runtime_error,
                       "int32 from int64 should throw");

    return true;
}

FATP_TEST_CASE(empty_buffer)
{
    std::vector<std::uint8_t> empty;

    FATP_ASSERT_THROWS(([&] {
                           Decoder dec(empty);
                           (void)dec.readUint64();
                       }()),
                       std::runtime_error,
                       "empty buffer uint64 should throw");

    FATP_ASSERT_THROWS(([&] {
                           Decoder dec(empty);
                           (void)dec.readString();
                       }()),
                       std::runtime_error,
                       "empty buffer string should throw");

    FATP_ASSERT_THROWS(([&] {
                           Decoder dec(empty);
                           (void)dec.peekType();
                       }()),
                       std::runtime_error,
                       "empty buffer peek should throw");

    return true;
}

FATP_TEST_CASE(invalid_type_tag)
{
    std::vector<std::uint8_t> bad = {15U, 0U, 0U, 0U, 0U};

    FATP_ASSERT_THROWS(([&] {
                           Decoder dec(bad);
                           (void)dec.readUint32();
                       }()),
                       std::runtime_error,
                       "invalid type tag should throw");

    return true;
}

FATP_TEST_CASE(impossible_string_length)
{
    std::vector<std::uint8_t> buf;
    buf.push_back(static_cast<std::uint8_t>(TypeTag::String));

    std::uint64_t len = 1000000ULL;
    const auto* len_bytes = reinterpret_cast<const std::uint8_t*>(&len);
    buf.insert(buf.end(), len_bytes, len_bytes + sizeof(len));

    buf.push_back('a');
    buf.push_back('b');
    buf.push_back('c');

    FATP_ASSERT_THROWS(([&] {
                           Decoder dec(buf);
                           (void)dec.readString();
                       }()),
                       std::runtime_error,
                       "impossible string length should throw");

    return true;
}

FATP_TEST_CASE(read_past_eof)
{
    std::vector<std::uint8_t> buf;
    Encoder enc(buf);
    enc.writeUint32(42U);

    Decoder dec(buf);
    (void)dec.readUint32();

    FATP_ASSERT_TRUE(dec.eof(), "should be at EOF");

    FATP_ASSERT_THROWS(([&] {
                           (void)dec.readUint32();
                       }()),
                       std::runtime_error,
                       "reading past EOF should throw");

    return true;
}

// ============================================================================
// Fuzz Tests
// ============================================================================

FATP_TEST_CASE(fuzz_uint64)
{
    std::mt19937_64 rng(0xB1A2B3C4D5E6F7A8ULL);

    for (int i = 0; i < 2000; ++i)
    {
        const auto v = rng();

        std::vector<std::uint8_t> buf;
        Encoder enc(buf);
        enc.writeUint64(v);

        Decoder dec(buf);
        FATP_ASSERT_EQ(dec.readUint64(), v, "fuzz uint64");
    }

    return true;
}

FATP_TEST_CASE(fuzz_int64)
{
    std::mt19937_64 rng(0xB1A2B3C4D5E6F7A9ULL);
    std::uniform_int_distribution<std::int64_t> dist(std::numeric_limits<std::int64_t>::min(),
                                                     std::numeric_limits<std::int64_t>::max());

    for (int i = 0; i < 2000; ++i)
    {
        const auto v = dist(rng);

        std::vector<std::uint8_t> buf;
        Encoder enc(buf);
        enc.writeInt64(v);

        Decoder dec(buf);
        FATP_ASSERT_EQ(dec.readInt64(), v, "fuzz int64");
    }

    return true;
}

FATP_TEST_CASE(fuzz_double)
{
    std::mt19937_64 rng(0xB1A2B3C4D5E6F7AAULL);
    std::uniform_real_distribution<double> dist(-1e15, 1e15);

    for (int i = 0; i < 2000; ++i)
    {
        const double v = dist(rng);

        std::vector<std::uint8_t> buf;
        Encoder enc(buf);
        enc.writeDouble(v);

        Decoder dec(buf);
        FATP_ASSERT_EQ(dec.readDouble(), v, "fuzz double");
    }

    return true;
}

FATP_TEST_CASE(fuzz_string)
{
    std::mt19937_64 rng(0xB1A2B3C4D5E6F7ABULL);
    std::uniform_int_distribution<std::size_t> len_dist(0, 128);
    std::uniform_int_distribution<int> ch_dist(32, 126);

    for (int i = 0; i < 2000; ++i)
    {
        const std::size_t len = len_dist(rng);
        std::string s;
        s.reserve(len);
        for (std::size_t j = 0; j < len; ++j)
        {
            s.push_back(static_cast<char>(ch_dist(rng)));
        }

        std::vector<std::uint8_t> buf;
        Encoder enc(buf);
        enc.writeString(s);

        Decoder dec(buf);
        FATP_ASSERT_EQ(dec.readString(), s, "fuzz string");
    }

    return true;
}

FATP_TEST_CASE(fuzz_bytes)
{
    std::mt19937_64 rng(0xB1A2B3C4D5E6F7ACULL);
    std::uniform_int_distribution<std::size_t> len_dist(0, 256);
    std::uniform_int_distribution<int> byte_dist(0, 255);

    for (int i = 0; i < 1000; ++i)
    {
        const std::size_t len = len_dist(rng);
        std::vector<std::uint8_t> payload;
        payload.reserve(len);
        for (std::size_t j = 0; j < len; ++j)
        {
            payload.push_back(static_cast<std::uint8_t>(byte_dist(rng)));
        }

        std::vector<std::uint8_t> buf;
        Encoder enc(buf);
        enc.writeBytes(payload);

        Decoder dec(buf);
        const auto result = dec.readBytes();
        FATP_ASSERT_EQ(result.size(), payload.size(), "fuzz bytes size");
        for (std::size_t j = 0; j < payload.size(); ++j)
        {
            FATP_ASSERT_EQ(result[j], payload[j], "fuzz bytes element");
        }
    }

    return true;
}

FATP_TEST_CASE(fuzz_multiple_values)
{
    std::mt19937_64 rng(0xB1A2B3C4D5E6F7ADULL);
    std::uniform_int_distribution<std::int64_t> val_dist(-1000000, 1000000);

    for (int iter = 0; iter < 500; ++iter)
    {
        std::uniform_int_distribution<std::size_t> count_dist(1, 20);
        const std::size_t count = count_dist(rng);

        std::vector<std::uint8_t> buf;
        Encoder enc(buf);

        std::vector<std::int64_t> values;
        values.reserve(count);
        for (std::size_t i = 0; i < count; ++i)
        {
            const auto v = val_dist(rng);
            values.push_back(v);
            enc.writeInt64(v);
        }

        Decoder dec(buf);
        for (std::size_t i = 0; i < count; ++i)
        {
            FATP_ASSERT_EQ(dec.readInt64(), values[i], "fuzz multiple values");
        }
        FATP_ASSERT_TRUE(dec.eof(), "should be at EOF");
    }

    return true;
}

FATP_TEST_CASE(fuzz_mixed_types)
{
    std::mt19937_64 rng(0xB1A2B3C4D5E6F7AEULL);
    std::uniform_int_distribution<int> type_dist(0, 5);
    std::uniform_int_distribution<std::int64_t> int_dist(std::numeric_limits<std::int64_t>::min(),
                                                         std::numeric_limits<std::int64_t>::max());
    std::uniform_real_distribution<double> dbl_dist(-1e10, 1e10);
    std::uniform_int_distribution<std::size_t> len_dist(0, 64);
    std::uniform_int_distribution<int> ch_dist(32, 126);

    for (int i = 0; i < 1000; ++i)
    {
        const int type = type_dist(rng);

        std::vector<std::uint8_t> buf;
        Encoder enc(buf);

        switch (type)
        {
            case 0:
            {
                const auto v = static_cast<std::uint64_t>(rng());
                enc.writeUint64(v);
                Decoder dec(buf);
                FATP_ASSERT_EQ(dec.readUint64(), v, "mixed uint64");
                break;
            }
            case 1:
            {
                const auto v = int_dist(rng);
                enc.writeInt64(v);
                Decoder dec(buf);
                FATP_ASSERT_EQ(dec.readInt64(), v, "mixed int64");
                break;
            }
            case 2:
            {
                const double v = dbl_dist(rng);
                enc.writeDouble(v);
                Decoder dec(buf);
                FATP_ASSERT_EQ(dec.readDouble(), v, "mixed double");
                break;
            }
            case 3:
            {
                const std::size_t len = len_dist(rng);
                std::string s;
                for (std::size_t j = 0; j < len; ++j)
                {
                    s.push_back(static_cast<char>(ch_dist(rng)));
                }
                enc.writeString(s);
                Decoder dec(buf);
                FATP_ASSERT_EQ(dec.readString(), s, "mixed string");
                break;
            }
            case 4:
            {
                const bool v = (rng() % 2) == 1;
                enc.writeBool(v);
                Decoder dec(buf);
                FATP_ASSERT_EQ(dec.readBool(), v, "mixed bool");
                break;
            }
            case 5:
            {
                std::uniform_real_distribution<float> flt_dist(-1e6f, 1e6f);
                const float v = flt_dist(rng);
                enc.writeFloat(v);
                Decoder dec(buf);
                FATP_ASSERT_EQ(dec.readFloat(), v, "mixed float");
                break;
            }
            default:
                break;
        }
    }

    return true;
}

// ============================================================================
// Benchmarks
// ============================================================================
} // namespace fat_p::testing::binarylite

namespace fat_p::testing
{


void run_benchmarks()
{
    using namespace fat_p::testing;

    auto& out = *get_test_config().output;
    out << "\n" << colors::cyan() << "Sanity Benchmark:" << colors::reset() << "\n";
    out << "  (benchmarks are provided by benchmark executables; unit tests do not run full benchmarks)\n\n";
}

bool test_BinaryLite()
{
    FATP_PRINT_HEADER(BINARY LITE)

    TestRunner runner;

    // Primitive roundtrips
    FATP_RUN_TEST_NS(runner, binarylite, uint8_roundtrip);
    FATP_RUN_TEST_NS(runner, binarylite, uint16_roundtrip);
    FATP_RUN_TEST_NS(runner, binarylite, uint32_roundtrip);
    FATP_RUN_TEST_NS(runner, binarylite, uint64_roundtrip);
    FATP_RUN_TEST_NS(runner, binarylite, int8_roundtrip);
    FATP_RUN_TEST_NS(runner, binarylite, int16_roundtrip);
    FATP_RUN_TEST_NS(runner, binarylite, int32_roundtrip);
    FATP_RUN_TEST_NS(runner, binarylite, int64_roundtrip);
    FATP_RUN_TEST_NS(runner, binarylite, bool_roundtrip);
    FATP_RUN_TEST_NS(runner, binarylite, float_roundtrip);
    FATP_RUN_TEST_NS(runner, binarylite, double_roundtrip);
    FATP_RUN_TEST_NS(runner, binarylite, string_roundtrip);
    FATP_RUN_TEST_NS(runner, binarylite, bytes_roundtrip);
    FATP_RUN_TEST_NS(runner, binarylite, array_header);
    FATP_RUN_TEST_NS(runner, binarylite, map_header);
    FATP_RUN_TEST_NS(runner, binarylite, peek_type);
    FATP_RUN_TEST_NS(runner, binarylite, multiple_values);

    // Malformed input tests
    FATP_RUN_TEST_NS(runner, binarylite, truncated_uint64);
    FATP_RUN_TEST_NS(runner, binarylite, truncated_uint32);
    FATP_RUN_TEST_NS(runner, binarylite, truncated_string);
    FATP_RUN_TEST_NS(runner, binarylite, truncated_bytes);
    FATP_RUN_TEST_NS(runner, binarylite, truncated_double);
    FATP_RUN_TEST_NS(runner, binarylite, wrong_type_uint_from_string);
    FATP_RUN_TEST_NS(runner, binarylite, wrong_type_string_from_uint);
    FATP_RUN_TEST_NS(runner, binarylite, wrong_type_bool_from_double);
    FATP_RUN_TEST_NS(runner, binarylite, wrong_type_int32_from_int64);
    FATP_RUN_TEST_NS(runner, binarylite, empty_buffer);
    FATP_RUN_TEST_NS(runner, binarylite, invalid_type_tag);
    FATP_RUN_TEST_NS(runner, binarylite, impossible_string_length);
    FATP_RUN_TEST_NS(runner, binarylite, read_past_eof);

    // Fuzz tests
    FATP_RUN_TEST_NS(runner, binarylite, fuzz_uint64);
    FATP_RUN_TEST_NS(runner, binarylite, fuzz_int64);
    FATP_RUN_TEST_NS(runner, binarylite, fuzz_double);
    FATP_RUN_TEST_NS(runner, binarylite, fuzz_string);
    FATP_RUN_TEST_NS(runner, binarylite, fuzz_bytes);
    FATP_RUN_TEST_NS(runner, binarylite, fuzz_multiple_values);
    FATP_RUN_TEST_NS(runner, binarylite, fuzz_mixed_types);


    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

// =============================================================================
// Standalone Entry Point
// =============================================================================
#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_BinaryLite() ? 0 : 1;
}
#endif
