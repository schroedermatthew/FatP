/**
 * @file test_CborLite.cpp
 * @brief Comprehensive unit tests for CborLite.h
 */
/*
FATP_META:
  meta_version: 1
  component: Cbor
  file_role: test
  path: components/Cbor/tests/test_CborLite.cpp
  layer: Testing
  namespace: fat_p
  summary: "Unit tests for CborLite."
  api_stability: in_work
  related:
    docs_search: "CborLite"
    headers:
      - include/fat_p/CborLite.h
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

#include "CborLite.h"
#include "FatPTest.h"

namespace fat_p::testing::cborlite
{

using fat_p::cbor::buffer;
using fat_p::cbor::byte;
using fat_p::cbor::Decoder;
using fat_p::cbor::Encoder;

// ============================================================================
// Primitive Roundtrips
// ============================================================================

FATP_TEST_CASE(uint_roundtrip)
{
    const std::uint64_t values[] = {0ULL,
                                    1ULL,
                                    23ULL,
                                    24ULL,
                                    255ULL,
                                    256ULL,
                                    65535ULL,
                                    65536ULL,
                                    0xFFFFFFFFULL,
                                    0x100000000ULL,
                                    123456789012345ULL,
                                    std::numeric_limits<std::uint64_t>::max()};

    for (std::uint64_t v : values)
    {
        buffer buf;
        Encoder enc(buf);
        enc.writeUint(v);

        Decoder dec(buf);
        FATP_ASSERT_EQ(dec.readUint(), v, "uint roundtrip");
    }

    return true;
}

FATP_TEST_CASE(int_positive_roundtrip)
{
    const std::int64_t values[] = {0, 1, 23, 24, 255, 256, 65535, 1234567, std::numeric_limits<std::int64_t>::max()};

    for (std::int64_t v : values)
    {
        buffer buf;
        Encoder enc(buf);
        enc.writeInt(v);

        Decoder dec(buf);
        FATP_ASSERT_EQ(dec.readInt(), v, "positive int roundtrip");
    }

    return true;
}

FATP_TEST_CASE(int_negative_roundtrip)
{
    const std::int64_t values[] =
        {-1, -23, -24, -255, -256, -65535, -9876543, std::numeric_limits<std::int64_t>::min()};

    for (std::int64_t v : values)
    {
        buffer buf;
        Encoder enc(buf);
        enc.writeInt(v);

        Decoder dec(buf);
        FATP_ASSERT_EQ(dec.readInt(), v, "negative int roundtrip");
    }

    return true;
}

FATP_TEST_CASE(bool_roundtrip)
{
    buffer buf;
    Encoder enc(buf);
    enc.writeBool(false);
    enc.writeBool(true);

    Decoder dec(buf);
    FATP_ASSERT_TRUE(!dec.readBool(), "false roundtrip");
    FATP_ASSERT_TRUE(dec.readBool(), "true roundtrip");

    return true;
}

FATP_TEST_CASE(null_roundtrip)
{
    buffer buf;
    Encoder enc(buf);
    enc.writeNull();

    Decoder dec(buf);
    dec.readNull();
    FATP_ASSERT_TRUE(dec.eof(), "should be at EOF after null");

    return true;
}

FATP_TEST_CASE(text_roundtrip)
{
    const std::string values[] = {"", "a", "hello", "UTF-8 \xC3\xA9\xC3\xA0", std::string(1000, 'x')};

    for (const auto& s : values)
    {
        buffer buf;
        Encoder enc(buf);
        enc.writeText(s);

        Decoder dec(buf);
        FATP_ASSERT_EQ(dec.readText(), s, "text roundtrip");
    }

    return true;
}

FATP_TEST_CASE(bytes_roundtrip)
{
    const buffer payload = {1U, 2U, 3U, 4U, 5U, 0U, 255U};

    buffer buf;
    Encoder enc(buf);
    enc.writeBytes(payload);

    Decoder dec(buf);
    const buffer out = dec.readBytes();
    FATP_ASSERT_EQ(out.size(), payload.size(), "bytes size");
    for (std::size_t i = 0; i < payload.size(); ++i)
    {
        FATP_ASSERT_EQ(out[i], payload[i], "bytes element");
    }

    return true;
}

FATP_TEST_CASE(empty_bytes_roundtrip)
{
    const buffer empty_payload;

    buffer buf;
    Encoder enc(buf);
    enc.writeBytes(empty_payload);

    Decoder dec(buf);
    const buffer out = dec.readBytes();
    FATP_ASSERT_TRUE(out.empty(), "empty bytes roundtrip");

    return true;
}

FATP_TEST_CASE(array_header)
{
    buffer buf;
    Encoder enc(buf);
    enc.beginArray(5U);

    Decoder dec(buf);
    FATP_ASSERT_EQ(dec.readArrayLength(), 5U, "array length");

    return true;
}

FATP_TEST_CASE(map_header)
{
    buffer buf;
    Encoder enc(buf);
    enc.beginMap(3U);

    Decoder dec(buf);
    FATP_ASSERT_EQ(dec.readMapLength(), 3U, "map length");

    return true;
}

FATP_TEST_CASE(multiple_values)
{
    buffer buf;
    Encoder enc(buf);

    enc.writeUint(42ULL);
    enc.writeInt(-123LL);
    enc.writeText("test");
    enc.writeBool(true);
    enc.writeNull();

    Decoder dec(buf);

    FATP_ASSERT_EQ(dec.readUint(), 42ULL, "uint");
    FATP_ASSERT_EQ(dec.readInt(), -123LL, "int");
    FATP_ASSERT_EQ(dec.readText(), "test", "text");
    FATP_ASSERT_TRUE(dec.readBool(), "bool");
    dec.readNull();
    FATP_ASSERT_TRUE(dec.eof(), "should be at EOF");

    return true;
}

// ============================================================================
// Malformed Input Tests
// ============================================================================

FATP_TEST_CASE(truncated_uint_1byte)
{
    buffer buf;
    Encoder enc(buf);
    enc.writeUint(255ULL);

    FATP_ASSERT_EQ(buf.size(), 2U, "uint8 should be 2 bytes");

    buffer truncated(buf.begin(), buf.begin() + 1);

    FATP_ASSERT_THROWS(([&] {
                           Decoder dec(truncated);
                           (void)dec.readUint();
                       }()),
                       std::runtime_error,
                       "truncated uint8 should throw");

    return true;
}

FATP_TEST_CASE(truncated_uint_2byte)
{
    buffer buf;
    Encoder enc(buf);
    enc.writeUint(1000ULL);

    for (std::size_t cut = 1; cut < buf.size(); ++cut)
    {
        buffer truncated(buf.begin(), buf.begin() + static_cast<std::ptrdiff_t>(cut));
        FATP_ASSERT_THROWS(([&] {
                               Decoder dec(truncated);
                               (void)dec.readUint();
                           }()),
                           std::runtime_error,
                           "truncated uint16 should throw");
    }

    return true;
}

FATP_TEST_CASE(truncated_uint_4byte)
{
    buffer buf;
    Encoder enc(buf);
    enc.writeUint(100000ULL);

    for (std::size_t cut = 1; cut < buf.size(); ++cut)
    {
        buffer truncated(buf.begin(), buf.begin() + static_cast<std::ptrdiff_t>(cut));
        FATP_ASSERT_THROWS(([&] {
                               Decoder dec(truncated);
                               (void)dec.readUint();
                           }()),
                           std::runtime_error,
                           "truncated uint32 should throw");
    }

    return true;
}

FATP_TEST_CASE(truncated_uint_8byte)
{
    buffer buf;
    Encoder enc(buf);
    enc.writeUint(0x100000000ULL);

    for (std::size_t cut = 1; cut < buf.size(); ++cut)
    {
        buffer truncated(buf.begin(), buf.begin() + static_cast<std::ptrdiff_t>(cut));
        FATP_ASSERT_THROWS(([&] {
                               Decoder dec(truncated);
                               (void)dec.readUint();
                           }()),
                           std::runtime_error,
                           "truncated uint64 should throw");
    }

    return true;
}

FATP_TEST_CASE(truncated_text)
{
    const std::string s = "Hello, World!";

    buffer buf;
    Encoder enc(buf);
    enc.writeText(s);

    for (std::size_t cut = 1; cut < buf.size(); ++cut)
    {
        buffer truncated(buf.begin(), buf.begin() + static_cast<std::ptrdiff_t>(cut));
        FATP_ASSERT_THROWS(([&] {
                               Decoder dec(truncated);
                               (void)dec.readText();
                           }()),
                           std::runtime_error,
                           "truncated text should throw");
    }

    return true;
}

FATP_TEST_CASE(truncated_bytes)
{
    const buffer payload = {1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U};

    buffer buf;
    Encoder enc(buf);
    enc.writeBytes(payload);

    for (std::size_t cut = 1; cut < buf.size(); ++cut)
    {
        buffer truncated(buf.begin(), buf.begin() + static_cast<std::ptrdiff_t>(cut));
        FATP_ASSERT_THROWS(([&] {
                               Decoder dec(truncated);
                               (void)dec.readBytes();
                           }()),
                           std::runtime_error,
                           "truncated bytes should throw");
    }

    return true;
}

FATP_TEST_CASE(wrong_type_uint_from_text)
{
    buffer buf;
    Encoder enc(buf);
    enc.writeText("not an integer");

    FATP_ASSERT_THROWS(([&] {
                           Decoder dec(buf);
                           (void)dec.readUint();
                       }()),
                       std::runtime_error,
                       "uint from text should throw");

    return true;
}

FATP_TEST_CASE(wrong_type_text_from_uint)
{
    buffer buf;
    Encoder enc(buf);
    enc.writeUint(12345ULL);

    FATP_ASSERT_THROWS(([&] {
                           Decoder dec(buf);
                           (void)dec.readText();
                       }()),
                       std::runtime_error,
                       "text from uint should throw");

    return true;
}

FATP_TEST_CASE(wrong_type_bool_from_int)
{
    buffer buf;
    Encoder enc(buf);
    enc.writeInt(1LL);

    FATP_ASSERT_THROWS(([&] {
                           Decoder dec(buf);
                           (void)dec.readBool();
                       }()),
                       std::runtime_error,
                       "bool from int should throw");

    return true;
}

FATP_TEST_CASE(wrong_type_array_from_map)
{
    buffer buf;
    Encoder enc(buf);
    enc.beginMap(3U);

    FATP_ASSERT_THROWS(([&] {
                           Decoder dec(buf);
                           (void)dec.readArrayLength();
                       }()),
                       std::runtime_error,
                       "array from map should throw");

    return true;
}

FATP_TEST_CASE(empty_buffer)
{
    buffer empty;

    FATP_ASSERT_THROWS(([&] {
                           Decoder dec(empty);
                           (void)dec.readUint();
                       }()),
                       std::runtime_error,
                       "empty buffer uint should throw");

    FATP_ASSERT_THROWS(([&] {
                           Decoder dec(empty);
                           (void)dec.readText();
                       }()),
                       std::runtime_error,
                       "empty buffer text should throw");

    FATP_ASSERT_THROWS(([&] {
                           Decoder dec(empty);
                           (void)dec.readBool();
                       }()),
                       std::runtime_error,
                       "empty buffer bool should throw");

    return true;
}

FATP_TEST_CASE(invalid_bool_value)
{
    buffer bad = {0xF7U};

    FATP_ASSERT_THROWS(([&] {
                           Decoder dec(bad);
                           (void)dec.readBool();
                       }()),
                       std::runtime_error,
                       "invalid bool value should throw");

    return true;
}

FATP_TEST_CASE(invalid_null_value)
{
    buffer bad = {0xF5U};

    FATP_ASSERT_THROWS(([&] {
                           Decoder dec(bad);
                           dec.readNull();
                       }()),
                       std::runtime_error,
                       "invalid null value should throw");

    return true;
}

FATP_TEST_CASE(indefinite_length_rejected)
{
    buffer bad = {0x5FU};

    FATP_ASSERT_THROWS(([&] {
                           Decoder dec(bad);
                           (void)dec.readBytes();
                       }()),
                       std::runtime_error,
                       "indefinite length should throw");

    return true;
}

FATP_TEST_CASE(impossible_text_length)
{
    buffer buf;
    buf.push_back(0x7AU);

    std::uint32_t len = 1000000U;
    buf.push_back(static_cast<byte>((len >> 24U) & 0xFFU));
    buf.push_back(static_cast<byte>((len >> 16U) & 0xFFU));
    buf.push_back(static_cast<byte>((len >> 8U) & 0xFFU));
    buf.push_back(static_cast<byte>(len & 0xFFU));

    buf.push_back('a');
    buf.push_back('b');
    buf.push_back('c');

    FATP_ASSERT_THROWS(([&] {
                           Decoder dec(buf);
                           (void)dec.readText();
                       }()),
                       std::runtime_error,
                       "impossible text length should throw");

    return true;
}

FATP_TEST_CASE(read_past_eof)
{
    buffer buf;
    Encoder enc(buf);
    enc.writeUint(42ULL);

    Decoder dec(buf);
    (void)dec.readUint();

    FATP_ASSERT_TRUE(dec.eof(), "should be at EOF");

    FATP_ASSERT_THROWS(([&] {
                           (void)dec.readUint();
                       }()),
                       std::runtime_error,
                       "reading past EOF should throw");

    return true;
}

// ============================================================================
// Fuzz Tests
// ============================================================================

FATP_TEST_CASE(fuzz_uint)
{
    std::mt19937_64 rng(0xCB0A1B2C3D4E5F6AULL);

    for (int i = 0; i < 2000; ++i)
    {
        const auto v = rng();

        buffer buf;
        Encoder enc(buf);
        enc.writeUint(v);

        Decoder dec(buf);
        FATP_ASSERT_EQ(dec.readUint(), v, "fuzz uint");
    }

    return true;
}

FATP_TEST_CASE(fuzz_int)
{
    std::mt19937_64 rng(0xCB0A1B2C3D4E5F6BULL);
    std::uniform_int_distribution<std::int64_t> dist(std::numeric_limits<std::int64_t>::min(),
                                                     std::numeric_limits<std::int64_t>::max());

    for (int i = 0; i < 2000; ++i)
    {
        const auto v = dist(rng);

        buffer buf;
        Encoder enc(buf);
        enc.writeInt(v);

        Decoder dec(buf);
        FATP_ASSERT_EQ(dec.readInt(), v, "fuzz int");
    }

    return true;
}

FATP_TEST_CASE(fuzz_text)
{
    std::mt19937_64 rng(0xCB0A1B2C3D4E5F6CULL);
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

        buffer buf;
        Encoder enc(buf);
        enc.writeText(s);

        Decoder dec(buf);
        FATP_ASSERT_EQ(dec.readText(), s, "fuzz text");
    }

    return true;
}

FATP_TEST_CASE(fuzz_bytes)
{
    std::mt19937_64 rng(0xCB0A1B2C3D4E5F6DULL);
    std::uniform_int_distribution<std::size_t> len_dist(0, 256);
    std::uniform_int_distribution<int> byte_dist(0, 255);

    for (int i = 0; i < 1000; ++i)
    {
        const std::size_t len = len_dist(rng);
        buffer payload;
        payload.reserve(len);
        for (std::size_t j = 0; j < len; ++j)
        {
            payload.push_back(static_cast<byte>(byte_dist(rng)));
        }

        buffer buf;
        Encoder enc(buf);
        enc.writeBytes(payload);

        Decoder dec(buf);
        const buffer result = dec.readBytes();
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
    std::mt19937_64 rng(0xCB0A1B2C3D4E5F6EULL);
    std::uniform_int_distribution<std::int64_t> val_dist(-1000000, 1000000);

    for (int iter = 0; iter < 500; ++iter)
    {
        std::uniform_int_distribution<std::size_t> count_dist(1, 20);
        const std::size_t count = count_dist(rng);

        buffer buf;
        Encoder enc(buf);

        std::vector<std::int64_t> values;
        values.reserve(count);
        for (std::size_t i = 0; i < count; ++i)
        {
            const auto v = val_dist(rng);
            values.push_back(v);
            enc.writeInt(v);
        }

        Decoder dec(buf);
        for (std::size_t i = 0; i < count; ++i)
        {
            FATP_ASSERT_EQ(dec.readInt(), values[i], "fuzz multiple values");
        }
        FATP_ASSERT_TRUE(dec.eof(), "should be at EOF");
    }

    return true;
}

FATP_TEST_CASE(fuzz_mixed_types)
{
    std::mt19937_64 rng(0xCB0A1B2C3D4E5F6FULL);
    std::uniform_int_distribution<int> type_dist(0, 4);
    std::uniform_int_distribution<std::int64_t> int_dist(std::numeric_limits<std::int64_t>::min(),
                                                         std::numeric_limits<std::int64_t>::max());
    std::uniform_int_distribution<std::size_t> len_dist(0, 64);
    std::uniform_int_distribution<int> ch_dist(32, 126);

    for (int i = 0; i < 1000; ++i)
    {
        const int type = type_dist(rng);

        buffer buf;
        Encoder enc(buf);

        switch (type)
        {
            case 0:
            {
                const auto v = static_cast<std::uint64_t>(rng());
                enc.writeUint(v);
                Decoder dec(buf);
                FATP_ASSERT_EQ(dec.readUint(), v, "mixed uint");
                break;
            }
            case 1:
            {
                const auto v = int_dist(rng);
                enc.writeInt(v);
                Decoder dec(buf);
                FATP_ASSERT_EQ(dec.readInt(), v, "mixed int");
                break;
            }
            case 2:
            {
                const std::size_t len = len_dist(rng);
                std::string s;
                for (std::size_t j = 0; j < len; ++j)
                {
                    s.push_back(static_cast<char>(ch_dist(rng)));
                }
                enc.writeText(s);
                Decoder dec(buf);
                FATP_ASSERT_EQ(dec.readText(), s, "mixed text");
                break;
            }
            case 3:
            {
                const bool v = (rng() % 2) == 1;
                enc.writeBool(v);
                Decoder dec(buf);
                FATP_ASSERT_EQ(dec.readBool(), v, "mixed bool");
                break;
            }
            case 4:
            {
                enc.writeNull();
                Decoder dec(buf);
                dec.readNull();
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
} // namespace fat_p::testing::cborlite

namespace fat_p::testing
{


void run_benchmarks()
{
    using namespace fat_p::testing;

    auto& out = *get_test_config().output;
    out << "\n" << colors::cyan() << "Sanity Benchmark:" << colors::reset() << "\n";
    out << "  (benchmarks are provided by benchmark executables; unit tests do not run full benchmarks)\n\n";
}

bool test_CborLite()
{
    FATP_PRINT_HEADER(CBOR LITE)

    TestRunner runner;

    // Primitive roundtrips
    FATP_RUN_TEST_NS(runner, cborlite, uint_roundtrip);
    FATP_RUN_TEST_NS(runner, cborlite, int_positive_roundtrip);
    FATP_RUN_TEST_NS(runner, cborlite, int_negative_roundtrip);
    FATP_RUN_TEST_NS(runner, cborlite, bool_roundtrip);
    FATP_RUN_TEST_NS(runner, cborlite, null_roundtrip);
    FATP_RUN_TEST_NS(runner, cborlite, text_roundtrip);
    FATP_RUN_TEST_NS(runner, cborlite, bytes_roundtrip);
    FATP_RUN_TEST_NS(runner, cborlite, empty_bytes_roundtrip);
    FATP_RUN_TEST_NS(runner, cborlite, array_header);
    FATP_RUN_TEST_NS(runner, cborlite, map_header);
    FATP_RUN_TEST_NS(runner, cborlite, multiple_values);

    // Malformed input tests
    FATP_RUN_TEST_NS(runner, cborlite, truncated_uint_1byte);
    FATP_RUN_TEST_NS(runner, cborlite, truncated_uint_2byte);
    FATP_RUN_TEST_NS(runner, cborlite, truncated_uint_4byte);
    FATP_RUN_TEST_NS(runner, cborlite, truncated_uint_8byte);
    FATP_RUN_TEST_NS(runner, cborlite, truncated_text);
    FATP_RUN_TEST_NS(runner, cborlite, truncated_bytes);
    FATP_RUN_TEST_NS(runner, cborlite, wrong_type_uint_from_text);
    FATP_RUN_TEST_NS(runner, cborlite, wrong_type_text_from_uint);
    FATP_RUN_TEST_NS(runner, cborlite, wrong_type_bool_from_int);
    FATP_RUN_TEST_NS(runner, cborlite, wrong_type_array_from_map);
    FATP_RUN_TEST_NS(runner, cborlite, empty_buffer);
    FATP_RUN_TEST_NS(runner, cborlite, invalid_bool_value);
    FATP_RUN_TEST_NS(runner, cborlite, invalid_null_value);
    FATP_RUN_TEST_NS(runner, cborlite, indefinite_length_rejected);
    FATP_RUN_TEST_NS(runner, cborlite, impossible_text_length);
    FATP_RUN_TEST_NS(runner, cborlite, read_past_eof);

    // Fuzz tests
    FATP_RUN_TEST_NS(runner, cborlite, fuzz_uint);
    FATP_RUN_TEST_NS(runner, cborlite, fuzz_int);
    FATP_RUN_TEST_NS(runner, cborlite, fuzz_text);
    FATP_RUN_TEST_NS(runner, cborlite, fuzz_bytes);
    FATP_RUN_TEST_NS(runner, cborlite, fuzz_multiple_values);
    FATP_RUN_TEST_NS(runner, cborlite, fuzz_mixed_types);


    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

// =============================================================================
// Standalone Entry Point
// =============================================================================
#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_CborLite() ? 0 : 1;
}
#endif
