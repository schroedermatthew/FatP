#include <cstdint>
#include <limits>
#include <random>
#include <string>
#include <vector>

#include "BinaryLite.h"
#include "FatPTest.h"

#ifndef ENABLE_TEST_APPLICATION
#include "test_BinaryLite.h"
#endif

namespace fat_p::testing::binarylite
{

using fat_p::binary::Encoder;
using fat_p::binary::Decoder;
using fat_p::binary::TypeTag;

// ============================================================================
// Primitive Roundtrips
// ============================================================================

TEST_CASE(uint8_roundtrip)
{
    const std::uint8_t values[] = {0U, 1U, 127U, 255U};

    for (std::uint8_t v : values)
    {
        std::vector<std::uint8_t> buf;
        Encoder enc(buf);
        enc.write_uint8(v);

        Decoder dec(buf);
        ASSERT_EQ(dec.read_uint8(), v, "uint8 roundtrip");
    }

    return true;
}

TEST_CASE(uint16_roundtrip)
{
    const std::uint16_t values[] = {0U, 1U, 255U, 256U, 65535U};

    for (std::uint16_t v : values)
    {
        std::vector<std::uint8_t> buf;
        Encoder enc(buf);
        enc.write_uint16(v);

        Decoder dec(buf);
        ASSERT_EQ(dec.read_uint16(), v, "uint16 roundtrip");
    }

    return true;
}

TEST_CASE(uint32_roundtrip)
{
    const std::uint32_t values[] = {0U, 1U, 65535U, 65536U, 0xDEADBEEFU};

    for (std::uint32_t v : values)
    {
        std::vector<std::uint8_t> buf;
        Encoder enc(buf);
        enc.write_uint32(v);

        Decoder dec(buf);
        ASSERT_EQ(dec.read_uint32(), v, "uint32 roundtrip");
    }

    return true;
}

TEST_CASE(uint64_roundtrip)
{
    const std::uint64_t values[] = {0ULL, 1ULL, 255ULL, 256ULL, 65535ULL,
                                    0xDEADBEEFCAFEBABEULL,
                                    std::numeric_limits<std::uint64_t>::max()};

    for (std::uint64_t v : values)
    {
        std::vector<std::uint8_t> buf;
        Encoder enc(buf);
        enc.write_uint64(v);

        Decoder dec(buf);
        ASSERT_EQ(dec.read_uint64(), v, "uint64 roundtrip");
    }

    return true;
}

TEST_CASE(int8_roundtrip)
{
    const std::int8_t values[] = {0, 1, -1, 127, -128};

    for (std::int8_t v : values)
    {
        std::vector<std::uint8_t> buf;
        Encoder enc(buf);
        enc.write_int8(v);

        Decoder dec(buf);
        ASSERT_EQ(dec.read_int8(), v, "int8 roundtrip");
    }

    return true;
}

TEST_CASE(int16_roundtrip)
{
    const std::int16_t values[] = {0, 1, -1, 32767, -32768};

    for (std::int16_t v : values)
    {
        std::vector<std::uint8_t> buf;
        Encoder enc(buf);
        enc.write_int16(v);

        Decoder dec(buf);
        ASSERT_EQ(dec.read_int16(), v, "int16 roundtrip");
    }

    return true;
}

TEST_CASE(int32_roundtrip)
{
    const std::int32_t values[] = {0, 1, -1, 1234567, -9876543,
                                   std::numeric_limits<std::int32_t>::min(),
                                   std::numeric_limits<std::int32_t>::max()};

    for (std::int32_t v : values)
    {
        std::vector<std::uint8_t> buf;
        Encoder enc(buf);
        enc.write_int32(v);

        Decoder dec(buf);
        ASSERT_EQ(dec.read_int32(), v, "int32 roundtrip");
    }

    return true;
}

TEST_CASE(int64_roundtrip)
{
    const std::int64_t values[] = {0, 1, -1, 1234567, -9876543,
                                   std::numeric_limits<std::int64_t>::min(),
                                   std::numeric_limits<std::int64_t>::max()};

    for (std::int64_t v : values)
    {
        std::vector<std::uint8_t> buf;
        Encoder enc(buf);
        enc.write_int64(v);

        Decoder dec(buf);
        ASSERT_EQ(dec.read_int64(), v, "int64 roundtrip");
    }

    return true;
}

TEST_CASE(bool_roundtrip)
{
    std::vector<std::uint8_t> buf;
    Encoder enc(buf);
    enc.write_bool(false);
    enc.write_bool(true);

    Decoder dec(buf);
    SIMPLE_ASSERT(!dec.read_bool(), "false roundtrip");
    SIMPLE_ASSERT(dec.read_bool(), "true roundtrip");

    return true;
}

TEST_CASE(float_roundtrip)
{
    const float values[] = {0.0f, -0.0f, 1.5f, -2.75f, 3.14159f};

    for (float v : values)
    {
        std::vector<std::uint8_t> buf;
        Encoder enc(buf);
        enc.write_float(v);

        Decoder dec(buf);
        ASSERT_EQ(dec.read_float(), v, "float roundtrip");
    }

    return true;
}

TEST_CASE(double_roundtrip)
{
    const double values[] = {0.0, -0.0, 1.5, -2.75, 1e10, -3.14159265358979};

    for (double v : values)
    {
        std::vector<std::uint8_t> buf;
        Encoder enc(buf);
        enc.write_double(v);

        Decoder dec(buf);
        ASSERT_EQ(dec.read_double(), v, "double roundtrip");
    }

    return true;
}

TEST_CASE(string_roundtrip)
{
    const std::string values[] = {"", "a", "hello", "UTF-8 \xC3\xA9\xC3\xA0"};

    for (const auto& s : values)
    {
        std::vector<std::uint8_t> buf;
        Encoder enc(buf);
        enc.write_string(s);

        Decoder dec(buf);
        ASSERT_EQ(dec.read_string(), s, "string roundtrip");
    }

    return true;
}

TEST_CASE(bytes_roundtrip)
{
    const std::vector<std::uint8_t> payload = {1U, 2U, 3U, 4U, 5U, 0U, 255U};

    std::vector<std::uint8_t> buf;
    Encoder enc(buf);
    enc.write_bytes(payload);

    Decoder dec(buf);
    const auto result = dec.read_bytes();
    ASSERT_EQ(result.size(), payload.size(), "bytes size");
    for (std::size_t i = 0; i < payload.size(); ++i)
    {
        ASSERT_EQ(result[i], payload[i], "bytes element");
    }

    return true;
}

TEST_CASE(array_header)
{
    std::vector<std::uint8_t> buf;
    Encoder enc(buf);
    enc.begin_array(5U);

    Decoder dec(buf);
    ASSERT_EQ(dec.read_array_length(), 5U, "array length");

    return true;
}

TEST_CASE(map_header)
{
    std::vector<std::uint8_t> buf;
    Encoder enc(buf);
    enc.begin_map(3U);

    Decoder dec(buf);
    ASSERT_EQ(dec.read_map_length(), 3U, "map length");

    return true;
}

TEST_CASE(peek_type)
{
    std::vector<std::uint8_t> buf;
    Encoder enc(buf);
    enc.write_uint32(42U);
    enc.write_string("hello");
    enc.write_bool(true);

    Decoder dec(buf);

    SIMPLE_ASSERT(dec.peek_type() == TypeTag::Uint32, "peek uint32");
    (void)dec.read_uint32();

    SIMPLE_ASSERT(dec.peek_type() == TypeTag::String, "peek string");
    (void)dec.read_string();

    SIMPLE_ASSERT(dec.peek_type() == TypeTag::Bool, "peek bool");
    (void)dec.read_bool();

    SIMPLE_ASSERT(dec.eof(), "should be at EOF");

    return true;
}

TEST_CASE(multiple_values)
{
    std::vector<std::uint8_t> buf;
    Encoder enc(buf);

    enc.write_uint32(42U);
    enc.write_int64(-123456789LL);
    enc.write_string("test");
    enc.write_double(3.14159);
    enc.write_bool(true);

    Decoder dec(buf);

    ASSERT_EQ(dec.read_uint32(), 42U, "uint32");
    ASSERT_EQ(dec.read_int64(), -123456789LL, "int64");
    ASSERT_EQ(dec.read_string(), "test", "string");
    ASSERT_EQ(dec.read_double(), 3.14159, "double");
    SIMPLE_ASSERT(dec.read_bool(), "bool");
    SIMPLE_ASSERT(dec.eof(), "should be at EOF");

    return true;
}

// ============================================================================
// Malformed Input Tests
// ============================================================================

TEST_CASE(truncated_uint64)
{
    std::vector<std::uint8_t> buf;
    Encoder enc(buf);
    enc.write_uint64(0xDEADBEEFCAFEBABEULL);

    ASSERT_EQ(buf.size(), 9U, "uint64 should be 1 tag + 8 bytes");

    for (std::size_t cut = 1; cut < buf.size(); ++cut)
    {
        std::vector<std::uint8_t> truncated(buf.begin(), buf.begin() + cut);
        ASSERT_THROWS(
            ([&] {
                Decoder dec(truncated);
                (void)dec.read_uint64();
            }()),
            std::runtime_error,
            "truncated uint64 should throw");
    }

    return true;
}

TEST_CASE(truncated_uint32)
{
    std::vector<std::uint8_t> buf;
    Encoder enc(buf);
    enc.write_uint32(0xDEADBEEFU);

    ASSERT_EQ(buf.size(), 5U, "uint32 should be 1 tag + 4 bytes");

    for (std::size_t cut = 1; cut < buf.size(); ++cut)
    {
        std::vector<std::uint8_t> truncated(buf.begin(), buf.begin() + cut);
        ASSERT_THROWS(
            ([&] {
                Decoder dec(truncated);
                (void)dec.read_uint32();
            }()),
            std::runtime_error,
            "truncated uint32 should throw");
    }

    return true;
}

TEST_CASE(truncated_string)
{
    std::vector<std::uint8_t> buf;
    Encoder enc(buf);
    enc.write_string("Hello, World!");

    for (std::size_t cut = 1; cut < buf.size(); ++cut)
    {
        std::vector<std::uint8_t> truncated(buf.begin(), buf.begin() + cut);
        ASSERT_THROWS(
            ([&] {
                Decoder dec(truncated);
                (void)dec.read_string();
            }()),
            std::runtime_error,
            "truncated string should throw");
    }

    return true;
}

TEST_CASE(truncated_bytes)
{
    const std::vector<std::uint8_t> payload = {1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U};

    std::vector<std::uint8_t> buf;
    Encoder enc(buf);
    enc.write_bytes(payload);

    for (std::size_t cut = 1; cut < buf.size(); ++cut)
    {
        std::vector<std::uint8_t> truncated(buf.begin(), buf.begin() + cut);
        ASSERT_THROWS(
            ([&] {
                Decoder dec(truncated);
                (void)dec.read_bytes();
            }()),
            std::runtime_error,
            "truncated bytes should throw");
    }

    return true;
}

TEST_CASE(truncated_double)
{
    std::vector<std::uint8_t> buf;
    Encoder enc(buf);
    enc.write_double(3.14159265358979);

    ASSERT_EQ(buf.size(), 9U, "double should be 1 tag + 8 bytes");

    for (std::size_t cut = 1; cut < buf.size(); ++cut)
    {
        std::vector<std::uint8_t> truncated(buf.begin(), buf.begin() + cut);
        ASSERT_THROWS(
            ([&] {
                Decoder dec(truncated);
                (void)dec.read_double();
            }()),
            std::runtime_error,
            "truncated double should throw");
    }

    return true;
}

TEST_CASE(wrong_type_uint_from_string)
{
    std::vector<std::uint8_t> buf;
    Encoder enc(buf);
    enc.write_string("not a number");

    ASSERT_THROWS(
        ([&] {
            Decoder dec(buf);
            (void)dec.read_uint64();
        }()),
        std::runtime_error,
        "uint64 from string should throw");

    return true;
}

TEST_CASE(wrong_type_string_from_uint)
{
    std::vector<std::uint8_t> buf;
    Encoder enc(buf);
    enc.write_uint64(12345ULL);

    ASSERT_THROWS(
        ([&] {
            Decoder dec(buf);
            (void)dec.read_string();
        }()),
        std::runtime_error,
        "string from uint should throw");

    return true;
}

TEST_CASE(wrong_type_bool_from_double)
{
    std::vector<std::uint8_t> buf;
    Encoder enc(buf);
    enc.write_double(1.0);

    ASSERT_THROWS(
        ([&] {
            Decoder dec(buf);
            (void)dec.read_bool();
        }()),
        std::runtime_error,
        "bool from double should throw");

    return true;
}

TEST_CASE(wrong_type_int32_from_int64)
{
    std::vector<std::uint8_t> buf;
    Encoder enc(buf);
    enc.write_int64(12345LL);

    ASSERT_THROWS(
        ([&] {
            Decoder dec(buf);
            (void)dec.read_int32();
        }()),
        std::runtime_error,
        "int32 from int64 should throw");

    return true;
}

TEST_CASE(empty_buffer)
{
    std::vector<std::uint8_t> empty;

    ASSERT_THROWS(
        ([&] {
            Decoder dec(empty);
            (void)dec.read_uint64();
        }()),
        std::runtime_error,
        "empty buffer uint64 should throw");

    ASSERT_THROWS(
        ([&] {
            Decoder dec(empty);
            (void)dec.read_string();
        }()),
        std::runtime_error,
        "empty buffer string should throw");

    ASSERT_THROWS(
        ([&] {
            Decoder dec(empty);
            (void)dec.peek_type();
        }()),
        std::runtime_error,
        "empty buffer peek should throw");

    return true;
}

TEST_CASE(invalid_type_tag)
{
    std::vector<std::uint8_t> bad = {15U, 0U, 0U, 0U, 0U};

    ASSERT_THROWS(
        ([&] {
            Decoder dec(bad);
            (void)dec.read_uint32();
        }()),
        std::runtime_error,
        "invalid type tag should throw");

    return true;
}

TEST_CASE(impossible_string_length)
{
    std::vector<std::uint8_t> buf;
    buf.push_back(static_cast<std::uint8_t>(TypeTag::String));

    std::uint64_t len = 1000000ULL;
    const auto* len_bytes = reinterpret_cast<const std::uint8_t*>(&len);
    buf.insert(buf.end(), len_bytes, len_bytes + sizeof(len));

    buf.push_back('a');
    buf.push_back('b');
    buf.push_back('c');

    ASSERT_THROWS(
        ([&] {
            Decoder dec(buf);
            (void)dec.read_string();
        }()),
        std::runtime_error,
        "impossible string length should throw");

    return true;
}

TEST_CASE(read_past_eof)
{
    std::vector<std::uint8_t> buf;
    Encoder enc(buf);
    enc.write_uint32(42U);

    Decoder dec(buf);
    (void)dec.read_uint32();

    SIMPLE_ASSERT(dec.eof(), "should be at EOF");

    ASSERT_THROWS(
        ([&] {
            (void)dec.read_uint32();
        }()),
        std::runtime_error,
        "reading past EOF should throw");

    return true;
}

// ============================================================================
// Fuzz Tests
// ============================================================================

TEST_CASE(fuzz_uint64)
{
    std::mt19937_64 rng(0xB1A2B3C4D5E6F7A8ULL);

    for (int i = 0; i < 2000; ++i)
    {
        const auto v = rng();

        std::vector<std::uint8_t> buf;
        Encoder enc(buf);
        enc.write_uint64(v);

        Decoder dec(buf);
        ASSERT_EQ(dec.read_uint64(), v, "fuzz uint64");
    }

    return true;
}

TEST_CASE(fuzz_int64)
{
    std::mt19937_64 rng(0xB1A2B3C4D5E6F7A9ULL);
    std::uniform_int_distribution<std::int64_t> dist(
        std::numeric_limits<std::int64_t>::min(),
        std::numeric_limits<std::int64_t>::max());

    for (int i = 0; i < 2000; ++i)
    {
        const auto v = dist(rng);

        std::vector<std::uint8_t> buf;
        Encoder enc(buf);
        enc.write_int64(v);

        Decoder dec(buf);
        ASSERT_EQ(dec.read_int64(), v, "fuzz int64");
    }

    return true;
}

TEST_CASE(fuzz_double)
{
    std::mt19937_64 rng(0xB1A2B3C4D5E6F7AAULL);
    std::uniform_real_distribution<double> dist(-1e15, 1e15);

    for (int i = 0; i < 2000; ++i)
    {
        const double v = dist(rng);

        std::vector<std::uint8_t> buf;
        Encoder enc(buf);
        enc.write_double(v);

        Decoder dec(buf);
        ASSERT_EQ(dec.read_double(), v, "fuzz double");
    }

    return true;
}

TEST_CASE(fuzz_string)
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
        enc.write_string(s);

        Decoder dec(buf);
        ASSERT_EQ(dec.read_string(), s, "fuzz string");
    }

    return true;
}

TEST_CASE(fuzz_bytes)
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
        enc.write_bytes(payload);

        Decoder dec(buf);
        const auto result = dec.read_bytes();
        ASSERT_EQ(result.size(), payload.size(), "fuzz bytes size");
        for (std::size_t j = 0; j < payload.size(); ++j)
        {
            ASSERT_EQ(result[j], payload[j], "fuzz bytes element");
        }
    }

    return true;
}

TEST_CASE(fuzz_multiple_values)
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
            enc.write_int64(v);
        }

        Decoder dec(buf);
        for (std::size_t i = 0; i < count; ++i)
        {
            ASSERT_EQ(dec.read_int64(), values[i], "fuzz multiple values");
        }
        SIMPLE_ASSERT(dec.eof(), "should be at EOF");
    }

    return true;
}

TEST_CASE(fuzz_mixed_types)
{
    std::mt19937_64 rng(0xB1A2B3C4D5E6F7AEULL);
    std::uniform_int_distribution<int> type_dist(0, 5);
    std::uniform_int_distribution<std::int64_t> int_dist(
        std::numeric_limits<std::int64_t>::min(),
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
            enc.write_uint64(v);
            Decoder dec(buf);
            ASSERT_EQ(dec.read_uint64(), v, "mixed uint64");
            break;
        }
        case 1:
        {
            const auto v = int_dist(rng);
            enc.write_int64(v);
            Decoder dec(buf);
            ASSERT_EQ(dec.read_int64(), v, "mixed int64");
            break;
        }
        case 2:
        {
            const double v = dbl_dist(rng);
            enc.write_double(v);
            Decoder dec(buf);
            ASSERT_EQ(dec.read_double(), v, "mixed double");
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
            enc.write_string(s);
            Decoder dec(buf);
            ASSERT_EQ(dec.read_string(), s, "mixed string");
            break;
        }
        case 4:
        {
            const bool v = (rng() % 2) == 1;
            enc.write_bool(v);
            Decoder dec(buf);
            ASSERT_EQ(dec.read_bool(), v, "mixed bool");
            break;
        }
        case 5:
        {
            std::uniform_real_distribution<float> flt_dist(-1e6f, 1e6f);
            const float v = flt_dist(rng);
            enc.write_float(v);
            Decoder dec(buf);
            ASSERT_EQ(dec.read_float(), v, "mixed float");
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

void benchmark_binarylite()
{
    std::cout << "\n" << colors::cyan() << "BinaryLite Benchmarks:" << colors::reset() << "\n\n";

    double enc_time = measure_perf([]()
    {
        std::vector<std::uint8_t> buf;
        buf.reserve(90000);
        Encoder enc(buf);
        for (std::uint64_t i = 0; i < 10000; ++i)
        {
            enc.write_uint64(i * 12345ULL);
        }
        DoNotOptimize(buf.data());
    }, 1000, 100);
    std::cout << "Encode 10000 uint64: " << format_time(enc_time) << "\n";

    std::vector<std::uint8_t> int_buf;
    int_buf.reserve(90000);
    {
        Encoder enc(int_buf);
        for (std::uint64_t i = 0; i < 10000; ++i)
        {
            enc.write_uint64(i * 12345ULL);
        }
    }

    double dec_time = measure_perf([&int_buf]()
    {
        Decoder dec(int_buf);
        std::uint64_t sum = 0;
        for (int i = 0; i < 10000; ++i)
        {
            sum += dec.read_uint64();
        }
        DoNotOptimize(sum);
    }, 1000, 100);
    std::cout << "Decode 10000 uint64: " << format_time(dec_time) << "\n";

    std::string large_str(100000, 'x');
    double str_enc_time = measure_perf([&large_str]()
    {
        std::vector<std::uint8_t> buf;
        Encoder enc(buf);
        enc.write_string(large_str);
        DoNotOptimize(buf.data());
    }, 1000, 100);
    std::cout << "Encode 100KB string: " << format_time(str_enc_time) << "\n";

    std::vector<std::uint8_t> str_buf;
    {
        Encoder enc(str_buf);
        enc.write_string(large_str);
    }

    double str_dec_time = measure_perf([&str_buf]()
    {
        Decoder dec(str_buf);
        auto s = dec.read_string();
        DoNotOptimize(s.data());
    }, 1000, 100);
    std::cout << "Decode 100KB string: " << format_time(str_dec_time) << "\n";

    std::vector<std::uint8_t> large_bytes(1000000);
    for (std::size_t i = 0; i < large_bytes.size(); ++i)
    {
        large_bytes[i] = static_cast<std::uint8_t>(i & 0xFF);
    }

    double bytes_enc_time = measure_perf([&large_bytes]()
    {
        std::vector<std::uint8_t> buf;
        Encoder enc(buf);
        enc.write_bytes(large_bytes);
        DoNotOptimize(buf.data());
    }, 100, 10);
    std::cout << "Encode 1MB bytes: " << format_time(bytes_enc_time) << "\n";

    std::vector<std::uint8_t> bytes_buf;
    {
        Encoder enc(bytes_buf);
        enc.write_bytes(large_bytes);
    }

    double bytes_dec_time = measure_perf([&bytes_buf]()
    {
        Decoder dec(bytes_buf);
        auto b = dec.read_bytes();
        DoNotOptimize(b.data());
    }, 100, 10);
    std::cout << "Decode 1MB bytes: " << format_time(bytes_dec_time) << "\n";
}

} // namespace fat_p::testing::binarylite

namespace fat_p::testing
{

bool test_BinaryLite()
{
    PRINT_HEADER(BINARY LITE)

    TestRunner runner;

    // Primitive roundtrips
    RUN_TEST_NS(runner, binarylite, uint8_roundtrip);
    RUN_TEST_NS(runner, binarylite, uint16_roundtrip);
    RUN_TEST_NS(runner, binarylite, uint32_roundtrip);
    RUN_TEST_NS(runner, binarylite, uint64_roundtrip);
    RUN_TEST_NS(runner, binarylite, int8_roundtrip);
    RUN_TEST_NS(runner, binarylite, int16_roundtrip);
    RUN_TEST_NS(runner, binarylite, int32_roundtrip);
    RUN_TEST_NS(runner, binarylite, int64_roundtrip);
    RUN_TEST_NS(runner, binarylite, bool_roundtrip);
    RUN_TEST_NS(runner, binarylite, float_roundtrip);
    RUN_TEST_NS(runner, binarylite, double_roundtrip);
    RUN_TEST_NS(runner, binarylite, string_roundtrip);
    RUN_TEST_NS(runner, binarylite, bytes_roundtrip);
    RUN_TEST_NS(runner, binarylite, array_header);
    RUN_TEST_NS(runner, binarylite, map_header);
    RUN_TEST_NS(runner, binarylite, peek_type);
    RUN_TEST_NS(runner, binarylite, multiple_values);

    // Malformed input tests
    RUN_TEST_NS(runner, binarylite, truncated_uint64);
    RUN_TEST_NS(runner, binarylite, truncated_uint32);
    RUN_TEST_NS(runner, binarylite, truncated_string);
    RUN_TEST_NS(runner, binarylite, truncated_bytes);
    RUN_TEST_NS(runner, binarylite, truncated_double);
    RUN_TEST_NS(runner, binarylite, wrong_type_uint_from_string);
    RUN_TEST_NS(runner, binarylite, wrong_type_string_from_uint);
    RUN_TEST_NS(runner, binarylite, wrong_type_bool_from_double);
    RUN_TEST_NS(runner, binarylite, wrong_type_int32_from_int64);
    RUN_TEST_NS(runner, binarylite, empty_buffer);
    RUN_TEST_NS(runner, binarylite, invalid_type_tag);
    RUN_TEST_NS(runner, binarylite, impossible_string_length);
    RUN_TEST_NS(runner, binarylite, read_past_eof);

    // Fuzz tests
    RUN_TEST_NS(runner, binarylite, fuzz_uint64);
    RUN_TEST_NS(runner, binarylite, fuzz_int64);
    RUN_TEST_NS(runner, binarylite, fuzz_double);
    RUN_TEST_NS(runner, binarylite, fuzz_string);
    RUN_TEST_NS(runner, binarylite, fuzz_bytes);
    RUN_TEST_NS(runner, binarylite, fuzz_multiple_values);
    RUN_TEST_NS(runner, binarylite, fuzz_mixed_types);

    binarylite::benchmark_binarylite();

    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_BinaryLite() ? 0 : 1;
}
#endif
