/**
 * @file test_FatPBinary.cpp
 * @brief Comprehensive unit tests for FatPBinary.h
 */

#include <cmath>
#include <cstdint>
#include <limits>
#include <map>
#include <random>
#include <string>
#include <vector>

#include "FatPBinary.h"
#include "FatPTest.h"

namespace fat_p::testing::fatpbinary
{

using fat_p::binary_fatp::BinaryBuffer;
using fat_p::binary_fatp::BinaryWriter;
using fat_p::binary_fatp::BinaryReader;
using fat_p::binary_fatp::BinaryResult;
using fat_p::binary_fatp::BinaryError;
using fat_p::binary_fatp::binary_encode_to;
using fat_p::binary_fatp::binary_decode_from;

// ============================================================================
// Buffer Alignment
// ============================================================================

TEST_CASE(buffer_alignment)
{
    BinaryBuffer buf;
    buf.reserve(64);

    const auto addr = reinterpret_cast<std::uintptr_t>(buf.data());
    ASSERT_TRUE(addr % 64 == 0, "BinaryBuffer must be 64-byte aligned");

    return true;
}

// ============================================================================
// Primitive Roundtrips via Traits
// ============================================================================

TEST_CASE(int_roundtrip)
{
    const int values[] = {0, 1, -1, 123456, -987654,
                          std::numeric_limits<int>::min(),
                          std::numeric_limits<int>::max()};

    for (int v : values)
    {
        BinaryBuffer buf;
        auto enc = binary_encode_to(buf, v);
        ASSERT_TRUE(enc.has_value(), "Encode int failed");

        auto dec = binary_decode_from<int>(buf);
        ASSERT_TRUE(dec.has_value(), dec.error().message.c_str());
        ASSERT_EQ(*dec, v, "int roundtrip");
    }

    return true;
}

TEST_CASE(uint64_roundtrip)
{
    const std::uint64_t values[] = {0ULL, 1ULL, 23ULL, 65535ULL, 123456789ULL,
                                    std::numeric_limits<std::uint64_t>::max()};

    for (std::uint64_t v : values)
    {
        BinaryBuffer buf;
        auto enc = binary_encode_to(buf, v);
        ASSERT_TRUE(enc.has_value(), "Encode uint64 failed");

        auto dec = binary_decode_from<std::uint64_t>(buf);
        ASSERT_TRUE(dec.has_value(), dec.error().message.c_str());
        ASSERT_EQ(*dec, v, "uint64 roundtrip");
    }

    return true;
}

TEST_CASE(bool_roundtrip)
{
    {
        BinaryBuffer buf;
        auto enc = binary_encode_to(buf, true);
        ASSERT_TRUE(enc.has_value(), "Encode true failed");

        auto dec = binary_decode_from<bool>(buf);
        ASSERT_TRUE(dec.has_value() && *dec == true, "true roundtrip");
    }

    {
        BinaryBuffer buf;
        auto enc = binary_encode_to(buf, false);
        ASSERT_TRUE(enc.has_value(), "Encode false failed");

        auto dec = binary_decode_from<bool>(buf);
        ASSERT_TRUE(dec.has_value() && *dec == false, "false roundtrip");
    }

    return true;
}

TEST_CASE(float_roundtrip)
{
    const float values[] = {0.0f, -0.0f, 1.5f, -2.75f, 3.14159f};

    for (float v : values)
    {
        BinaryBuffer buf;
        auto enc = binary_encode_to(buf, v);
        ASSERT_TRUE(enc.has_value(), "Encode float failed");

        auto dec = binary_decode_from<float>(buf);
        ASSERT_TRUE(dec.has_value(), dec.error().message.c_str());
        ASSERT_EQ(*dec, v, "float roundtrip");
    }

    return true;
}

TEST_CASE(double_roundtrip)
{
    const double values[] = {0.0, -0.0, 1.5, -2.75, 1e10, -3.14};

    for (double v : values)
    {
        BinaryBuffer buf;
        auto enc = binary_encode_to(buf, v);
        ASSERT_TRUE(enc.has_value(), "Encode double failed");

        auto dec = binary_decode_from<double>(buf);
        ASSERT_TRUE(dec.has_value(), dec.error().message.c_str());
        ASSERT_CLOSE_EPS(*dec, v, 1e-12, "double roundtrip");
    }

    return true;
}

TEST_CASE(string_roundtrip)
{
    const std::string values[] = {"", "a", "hello world", "UTF-8 \xC3\xA9"};

    for (const auto& s : values)
    {
        BinaryBuffer buf;
        auto enc = binary_encode_to(buf, s);
        ASSERT_TRUE(enc.has_value(), "Encode string failed");

        auto dec = binary_decode_from<std::string>(buf);
        ASSERT_TRUE(dec.has_value(), dec.error().message.c_str());
        ASSERT_EQ(*dec, s, "string roundtrip");
    }

    return true;
}

// ============================================================================
// Vector Roundtrips
// ============================================================================

TEST_CASE(vector_int_roundtrip)
{
    std::vector<int> v = {1, 2, 3, 4, 5};

    BinaryBuffer buf;
    auto enc = binary_encode_to(buf, v);
    ASSERT_TRUE(enc.has_value(), "Encode vector failed");

    auto dec = binary_decode_from<std::vector<int>>(buf);
    ASSERT_TRUE(dec.has_value(), dec.error().message.c_str());
    ASSERT_EQ(dec->size(), v.size(), "vector size");
    for (std::size_t i = 0; i < v.size(); ++i)
    {
        ASSERT_EQ((*dec)[i], v[i], "vector element");
    }

    return true;
}

TEST_CASE(nested_vector_roundtrip)
{
    std::vector<std::vector<int>> v = {{1, 2}, {3, 4, 5}, {}, {6}};

    BinaryBuffer buf;
    auto enc = binary_encode_to(buf, v);
    ASSERT_TRUE(enc.has_value(), "Encode nested vector failed");

    auto dec = binary_decode_from<std::vector<std::vector<int>>>(buf);
    ASSERT_TRUE(dec.has_value(), dec.error().message.c_str());
    ASSERT_EQ(dec->size(), v.size(), "outer vector size");
    for (std::size_t i = 0; i < v.size(); ++i)
    {
        ASSERT_EQ((*dec)[i].size(), v[i].size(), "inner vector size");
    }

    return true;
}

TEST_CASE(empty_vector_roundtrip)
{
    std::vector<int> v = {};

    BinaryBuffer buf;
    auto enc = binary_encode_to(buf, v);
    ASSERT_TRUE(enc.has_value(), "Encode empty vector failed");

    auto dec = binary_decode_from<std::vector<int>>(buf);
    ASSERT_TRUE(dec.has_value(), dec.error().message.c_str());
    ASSERT_TRUE(dec->empty(), "empty vector roundtrip");

    return true;
}

TEST_CASE(vector_string_roundtrip)
{
    std::vector<std::string> v = {"one", "two", "three", "", "with spaces"};

    BinaryBuffer buf;
    auto enc = binary_encode_to(buf, v);
    ASSERT_TRUE(enc.has_value(), "Encode vector<string> failed");

    auto dec = binary_decode_from<std::vector<std::string>>(buf);
    ASSERT_TRUE(dec.has_value(), dec.error().message.c_str());
    ASSERT_EQ(dec->size(), v.size(), "vector<string> size");
    for (std::size_t i = 0; i < v.size(); ++i)
    {
        ASSERT_EQ((*dec)[i], v[i], "vector<string> element");
    }

    return true;
}

// ============================================================================
// Map Roundtrips
// ============================================================================

TEST_CASE(map_roundtrip)
{
    std::map<std::string, int> m{{"a", 1}, {"b", 2}, {"c", 3}};

    BinaryBuffer buf;
    auto enc = binary_encode_to(buf, m);
    ASSERT_TRUE(enc.has_value(), "Encode map failed");

    auto dec = binary_decode_from<std::map<std::string, int>>(buf);
    ASSERT_TRUE(dec.has_value(), dec.error().message.c_str());
    ASSERT_EQ(dec->size(), m.size(), "map size");
    ASSERT_EQ(dec->at("a"), 1, "map value a");
    ASSERT_EQ(dec->at("b"), 2, "map value b");

    return true;
}

TEST_CASE(nested_map_roundtrip)
{
    std::map<std::string, std::map<int, std::string>> m{
        {"x", {{1, "a"}, {2, "b"}}},
        {"y", {{3, "c"}}}
    };

    BinaryBuffer buf;
    auto enc = binary_encode_to(buf, m);
    ASSERT_TRUE(enc.has_value(), "Encode nested map failed");

    auto dec = binary_decode_from<std::map<std::string, std::map<int, std::string>>>(buf);
    ASSERT_TRUE(dec.has_value(), dec.error().message.c_str());
    ASSERT_EQ(dec->size(), m.size(), "outer map size");

    return true;
}

TEST_CASE(empty_map_roundtrip)
{
    std::map<std::string, int> m{};

    BinaryBuffer buf;
    auto enc = binary_encode_to(buf, m);
    ASSERT_TRUE(enc.has_value(), "Encode empty map failed");

    auto dec = binary_decode_from<std::map<std::string, int>>(buf);
    ASSERT_TRUE(dec.has_value(), dec.error().message.c_str());
    ASSERT_TRUE(dec->empty(), "empty map roundtrip");

    return true;
}

// ============================================================================
// Enum Roundtrip
// ============================================================================

enum class TestEnum : std::int32_t
{
    Alpha = 0,
    Beta = 1,
    Gamma = -1
};

TEST_CASE(enum_roundtrip)
{
    const TestEnum values[] = {TestEnum::Alpha, TestEnum::Beta, TestEnum::Gamma};

    for (TestEnum v : values)
    {
        BinaryBuffer buf;
        auto enc = binary_encode_to(buf, v);
        ASSERT_TRUE(enc.has_value(), "Encode enum failed");

        auto dec = binary_decode_from<TestEnum>(buf);
        ASSERT_TRUE(dec.has_value(), dec.error().message.c_str());
        ASSERT_TRUE(*dec == v, "enum roundtrip");
    }

    return true;
}

// ============================================================================
// Cross-Container Compatibility
// ============================================================================

TEST_CASE(cross_container)
{
    BinaryBuffer hbuf;
    auto enc = binary_encode_to(hbuf, std::string("test123"));
    ASSERT_TRUE(enc.has_value(), "Encode failed");

    std::vector<std::uint8_t> sbuf(hbuf.begin(), hbuf.end());

    auto r1 = binary_decode_from<std::string>(hbuf);
    ASSERT_TRUE(r1.has_value() && *r1 == "test123", "Decode from HpcVector");

    auto r2 = binary_decode_from<std::string>(sbuf);
    ASSERT_TRUE(r2.has_value() && *r2 == "test123", "Decode from std::vector");

    return true;
}

// ============================================================================
// Malformed Input Tests
// ============================================================================

TEST_CASE(decode_invalid_tag)
{
    std::vector<std::uint8_t> bad = {0xFFU};

    auto dec = binary_decode_from<int>(bad);
    ASSERT_TRUE(!dec.has_value(), "Invalid tag should fail");
    ASSERT_TRUE(!dec.error().message.empty(), "Error message should exist");

    return true;
}

TEST_CASE(decode_truncated_string)
{
    BinaryBuffer buf;
    BinaryWriter<BinaryBuffer> writer(buf);
    writer.write_string("abcdefghij");

    std::vector<std::uint8_t> truncated(buf.begin(), buf.begin() + 5);

    auto dec = binary_decode_from<std::string>(truncated);
    ASSERT_TRUE(!dec.has_value(), "Truncated string should fail");

    return true;
}

TEST_CASE(decode_truncated_vector)
{
    std::vector<int> v = {1, 2, 3, 4, 5};

    BinaryBuffer buf;
    auto enc = binary_encode_to(buf, v);
    ASSERT_TRUE(enc.has_value(), "Encode failed");

    for (std::size_t cut = 1; cut < buf.size(); ++cut)
    {
        std::vector<std::uint8_t> truncated(buf.begin(), buf.begin() + cut);
        auto dec = binary_decode_from<std::vector<int>>(truncated);
        ASSERT_TRUE(!dec.has_value(), "Truncated vector should fail");
    }

    return true;
}

TEST_CASE(decode_type_mismatch_string_as_int)
{
    BinaryBuffer buf;
    auto enc = binary_encode_to(buf, std::string("not an int"));
    ASSERT_TRUE(enc.has_value(), "Encode string failed");

    auto dec = binary_decode_from<int>(buf);
    ASSERT_TRUE(!dec.has_value(), "Type mismatch should fail");

    return true;
}

TEST_CASE(decode_type_mismatch_int_as_string)
{
    BinaryBuffer buf;
    auto enc = binary_encode_to(buf, 42);
    ASSERT_TRUE(enc.has_value(), "Encode int failed");

    auto dec = binary_decode_from<std::string>(buf);
    ASSERT_TRUE(!dec.has_value(), "Type mismatch should fail");

    return true;
}

TEST_CASE(decode_type_mismatch_double_as_bool)
{
    BinaryBuffer buf;
    auto enc = binary_encode_to(buf, 3.14);
    ASSERT_TRUE(enc.has_value(), "Encode double failed");

    auto dec = binary_decode_from<bool>(buf);
    ASSERT_TRUE(!dec.has_value(), "Type mismatch should fail");

    return true;
}

TEST_CASE(decode_empty_buffer)
{
    std::vector<std::uint8_t> empty;

    auto dec_int = binary_decode_from<int>(empty);
    ASSERT_TRUE(!dec_int.has_value(), "Empty buffer should fail for int");

    auto dec_str = binary_decode_from<std::string>(empty);
    ASSERT_TRUE(!dec_str.has_value(), "Empty buffer should fail for string");

    auto dec_vec = binary_decode_from<std::vector<int>>(empty);
    ASSERT_TRUE(!dec_vec.has_value(), "Empty buffer should fail for vector");

    return true;
}

TEST_CASE(decode_impossible_length)
{
    BinaryBuffer buf;
    auto enc = binary_encode_to(buf, std::string("abcd"));
    ASSERT_TRUE(enc.has_value(), "Encode failed");

    std::vector<std::uint8_t> mutated(buf.begin(), buf.end());

    if (mutated.size() >= 9)
    {
        mutated[1] = 0xFFU;
        mutated[2] = 0xFFU;
        mutated[3] = 0x00U;
        mutated[4] = 0x00U;
    }

    auto dec = binary_decode_from<std::string>(mutated);
    ASSERT_TRUE(!dec.has_value(), "Impossible length should fail");

    return true;
}

TEST_CASE(decode_partial_map)
{
    std::map<std::string, int> m{{"a", 1}, {"b", 2}};

    BinaryBuffer buf;
    auto enc = binary_encode_to(buf, m);
    ASSERT_TRUE(enc.has_value(), "Encode failed");

    std::size_t cut = buf.size() * 2 / 3;
    std::vector<std::uint8_t> truncated(buf.begin(), buf.begin() + cut);

    auto dec = binary_decode_from<std::map<std::string, int>>(truncated);
    ASSERT_TRUE(!dec.has_value(), "Partial map should fail");

    return true;
}

TEST_CASE(decode_nested_truncation)
{
    std::vector<std::vector<int>> nested = {{1, 2}, {3, 4, 5}, {6}};

    BinaryBuffer buf;
    auto enc = binary_encode_to(buf, nested);
    ASSERT_TRUE(enc.has_value(), "Encode failed");

    std::size_t cut = buf.size() / 2;
    std::vector<std::uint8_t> truncated(buf.begin(), buf.begin() + cut);

    auto dec = binary_decode_from<std::vector<std::vector<int>>>(truncated);
    ASSERT_TRUE(!dec.has_value(), "Truncated nested should fail");

    return true;
}

TEST_CASE(decode_huge_length_protection)
{
    std::vector<std::uint8_t> buf;

    buf.push_back(static_cast<std::uint8_t>(fat_p::binary::TypeTag::Array));

    std::uint64_t huge_len = 0xFFFFFFFFFFFFFFFFULL;
    const auto* len_bytes = reinterpret_cast<const std::uint8_t*>(&huge_len);
    buf.insert(buf.end(), len_bytes, len_bytes + sizeof(huge_len));

    auto dec = binary_decode_from<std::vector<int>>(buf);
    ASSERT_TRUE(!dec.has_value(), "Huge length should fail");

    return true;
}

// ============================================================================
// Fuzz Tests
// ============================================================================

TEST_CASE(fuzz_ints)
{
    std::mt19937_64 rng(0xFA7B1A2C3D4E5F6AULL);
    std::uniform_int_distribution<int> dist(
        std::numeric_limits<int>::min(),
        std::numeric_limits<int>::max());

    for (int i = 0; i < 2000; ++i)
    {
        const int v = dist(rng);

        BinaryBuffer buf;
        auto enc = binary_encode_to(buf, v);
        ASSERT_TRUE(enc.has_value(), "Encode int failed");

        auto dec = binary_decode_from<int>(buf);
        ASSERT_TRUE(dec.has_value(), dec.error().message.c_str());
        ASSERT_EQ(*dec, v, "fuzz int");
    }

    return true;
}

TEST_CASE(fuzz_doubles)
{
    std::mt19937_64 rng(0xFA7B1A2C3D4E5F6BULL);
    std::uniform_real_distribution<double> dist(-1e6, 1e6);

    for (int i = 0; i < 2000; ++i)
    {
        const double v = dist(rng);

        BinaryBuffer buf;
        auto enc = binary_encode_to(buf, v);
        ASSERT_TRUE(enc.has_value(), "Encode double failed");

        auto dec = binary_decode_from<double>(buf);
        ASSERT_TRUE(dec.has_value(), dec.error().message.c_str());
        ASSERT_CLOSE_EPS(*dec, v, 1e-9, "fuzz double");
    }

    return true;
}

TEST_CASE(fuzz_strings)
{
    std::mt19937_64 rng(0xFA7B1A2C3D4E5F6CULL);
    std::uniform_int_distribution<std::size_t> len_dist(0, 64);
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

        BinaryBuffer buf;
        auto enc = binary_encode_to(buf, s);
        ASSERT_TRUE(enc.has_value(), "Encode string failed");

        auto dec = binary_decode_from<std::string>(buf);
        ASSERT_TRUE(dec.has_value(), dec.error().message.c_str());
        ASSERT_EQ(*dec, s, "fuzz string");
    }

    return true;
}

TEST_CASE(fuzz_vector_int)
{
    std::mt19937_64 rng(0xFA7B1A2C3D4E5F6DULL);
    std::uniform_int_distribution<std::size_t> len_dist(0, 32);
    std::uniform_int_distribution<int> val_dist(-100000, 100000);

    for (int iter = 0; iter < 1000; ++iter)
    {
        const std::size_t len = len_dist(rng);
        std::vector<int> v;
        v.reserve(len);
        for (std::size_t i = 0; i < len; ++i)
        {
            v.push_back(val_dist(rng));
        }

        BinaryBuffer buf;
        auto enc = binary_encode_to(buf, v);
        ASSERT_TRUE(enc.has_value(), "Encode vector failed");

        auto dec = binary_decode_from<std::vector<int>>(buf);
        ASSERT_TRUE(dec.has_value(), dec.error().message.c_str());
        ASSERT_EQ(dec->size(), v.size(), "fuzz vector size");
        for (std::size_t i = 0; i < v.size(); ++i)
        {
            ASSERT_EQ((*dec)[i], v[i], "fuzz vector element");
        }
    }

    return true;
}

TEST_CASE(fuzz_map_string_int)
{
    std::mt19937_64 rng(0xFA7B1A2C3D4E5F6EULL);
    std::uniform_int_distribution<std::size_t> len_dist(0, 16);
    std::uniform_int_distribution<std::size_t> key_len_dist(1, 16);
    std::uniform_int_distribution<int> ch_dist(97, 122);
    std::uniform_int_distribution<int> val_dist(-1000, 1000);

    for (int iter = 0; iter < 500; ++iter)
    {
        const std::size_t count = len_dist(rng);
        std::map<std::string, int> m;

        for (std::size_t i = 0; i < count; ++i)
        {
            const std::size_t key_len = key_len_dist(rng);
            std::string key;
            key.reserve(key_len);
            for (std::size_t j = 0; j < key_len; ++j)
            {
                key.push_back(static_cast<char>(ch_dist(rng)));
            }
            m[key] = val_dist(rng);
        }

        BinaryBuffer buf;
        auto enc = binary_encode_to(buf, m);
        ASSERT_TRUE(enc.has_value(), "Encode map failed");

        auto dec = binary_decode_from<std::map<std::string, int>>(buf);
        ASSERT_TRUE(dec.has_value(), dec.error().message.c_str());
        ASSERT_EQ(dec->size(), m.size(), "fuzz map size");
    }

    return true;
}

TEST_CASE(fuzz_nested_structures)
{
    std::mt19937_64 rng(0xFA7B1A2C3D4E5F6FULL);
    std::uniform_int_distribution<std::size_t> outer_dist(0, 8);
    std::uniform_int_distribution<std::size_t> inner_dist(0, 8);
    std::uniform_int_distribution<std::size_t> key_len_dist(1, 10);
    std::uniform_int_distribution<int> ch_dist(97, 122);
    std::uniform_int_distribution<int> val_dist(-5000, 5000);

    for (int iter = 0; iter < 300; ++iter)
    {
        const std::size_t outer_len = outer_dist(rng);
        std::vector<std::map<std::string, int>> v;
        v.reserve(outer_len);

        for (std::size_t i = 0; i < outer_len; ++i)
        {
            std::map<std::string, int> inner;
            const std::size_t inner_len = inner_dist(rng);

            for (std::size_t j = 0; j < inner_len; ++j)
            {
                const std::size_t key_len = key_len_dist(rng);
                std::string key;
                key.reserve(key_len);
                for (std::size_t k = 0; k < key_len; ++k)
                {
                    key.push_back(static_cast<char>(ch_dist(rng)));
                }
                inner[key] = val_dist(rng);
            }

            v.push_back(std::move(inner));
        }

        BinaryBuffer buf;
        auto enc = binary_encode_to(buf, v);
        ASSERT_TRUE(enc.has_value(), "Encode nested failed");

        auto dec = binary_decode_from<std::vector<std::map<std::string, int>>>(buf);
        ASSERT_TRUE(dec.has_value(), dec.error().message.c_str());
        ASSERT_EQ(dec->size(), v.size(), "fuzz nested outer size");
    }

    return true;
}

// ============================================================================
// Benchmarks
// ============================================================================

void benchmark_fatpbinary()
{
    std::cout << "\n" << colors::cyan() << "FatPBinary Benchmarks:" << colors::reset() << "\n\n";

    std::vector<int> vec;
    vec.reserve(10000);
    for (int i = 0; i < 10000; ++i)
    {
        vec.push_back(i);
    }

    double enc_time = measure_perf([&vec]()
    {
        BinaryBuffer buf;
        auto rc = binary_encode_to(buf, vec);
        DoNotOptimize(buf.data());
        (void)rc;
    }, 1000, 100);
    std::cout << "Encode vector<int> (10000): " << format_time(enc_time) << "\n";

    BinaryBuffer vec_buf;
    (void)binary_encode_to(vec_buf, vec);

    double dec_time = measure_perf([&vec_buf]()
    {
        auto rc = binary_decode_from<std::vector<int>>(vec_buf);
        DoNotOptimize(rc);
        (void)rc;
    }, 1000, 100);
    std::cout << "Decode vector<int> (10000): " << format_time(dec_time) << "\n";
    std::cout << "Encoded size: " << vec_buf.size() << " bytes\n";

    std::map<std::string, int> m;
    for (int i = 0; i < 1000; ++i)
    {
        m.emplace("key_" + std::to_string(i), i);
    }

    double map_enc_time = measure_perf([&m]()
    {
        BinaryBuffer buf;
        auto rc = binary_encode_to(buf, m);
        DoNotOptimize(buf.data());
        (void)rc;
    }, 500, 50);
    std::cout << "Encode map<string,int> (1000): " << format_time(map_enc_time) << "\n";

    BinaryBuffer map_buf;
    (void)binary_encode_to(map_buf, m);

    double map_dec_time = measure_perf([&map_buf]()
    {
        auto rc = binary_decode_from<std::map<std::string, int>>(map_buf);
        DoNotOptimize(rc);
        (void)rc;
    }, 500, 50);
    std::cout << "Decode map<string,int> (1000): " << format_time(map_dec_time) << "\n";
    std::cout << "Encoded size: " << map_buf.size() << " bytes\n";

    std::vector<std::map<std::string, int>> nested;
    nested.reserve(64);
    for (int i = 0; i < 64; ++i)
    {
        std::map<std::string, int> inner;
        for (int j = 0; j < 64; ++j)
        {
            inner.emplace("k_" + std::to_string(i) + "_" + std::to_string(j),
                          i * 1000 + j);
        }
        nested.push_back(std::move(inner));
    }

    double nested_enc_time = measure_perf([&nested]()
    {
        BinaryBuffer buf;
        auto rc = binary_encode_to(buf, nested);
        DoNotOptimize(buf.data());
        (void)rc;
    }, 100, 10);
    std::cout << "Encode nested (64x64): " << format_time(nested_enc_time) << "\n";

    BinaryBuffer nested_buf;
    (void)binary_encode_to(nested_buf, nested);

    double nested_dec_time = measure_perf([&nested_buf]()
    {
        auto rc =
            binary_decode_from<std::vector<std::map<std::string, int>>>(nested_buf);
        DoNotOptimize(rc);
        (void)rc;
    }, 100, 10);
    std::cout << "Decode nested (64x64): " << format_time(nested_dec_time) << "\n";
    std::cout << "Encoded size: " << nested_buf.size() << " bytes\n";
}

} // namespace fat_p::testing::fatpbinary

namespace fat_p::testing
{

bool test_FatPBinary()
{
    PRINT_HEADER(FATP BINARY)

    TestRunner runner;

    // Buffer and primitives
    RUN_TEST_NS(runner, fatpbinary, buffer_alignment);
    RUN_TEST_NS(runner, fatpbinary, int_roundtrip);
    RUN_TEST_NS(runner, fatpbinary, uint64_roundtrip);
    RUN_TEST_NS(runner, fatpbinary, bool_roundtrip);
    RUN_TEST_NS(runner, fatpbinary, float_roundtrip);
    RUN_TEST_NS(runner, fatpbinary, double_roundtrip);
    RUN_TEST_NS(runner, fatpbinary, string_roundtrip);

    // Containers
    RUN_TEST_NS(runner, fatpbinary, vector_int_roundtrip);
    RUN_TEST_NS(runner, fatpbinary, nested_vector_roundtrip);
    RUN_TEST_NS(runner, fatpbinary, empty_vector_roundtrip);
    RUN_TEST_NS(runner, fatpbinary, vector_string_roundtrip);
    RUN_TEST_NS(runner, fatpbinary, map_roundtrip);
    RUN_TEST_NS(runner, fatpbinary, nested_map_roundtrip);
    RUN_TEST_NS(runner, fatpbinary, empty_map_roundtrip);
    RUN_TEST_NS(runner, fatpbinary, enum_roundtrip);
    RUN_TEST_NS(runner, fatpbinary, cross_container);

    // Malformed input
    RUN_TEST_NS(runner, fatpbinary, decode_invalid_tag);
    RUN_TEST_NS(runner, fatpbinary, decode_truncated_string);
    RUN_TEST_NS(runner, fatpbinary, decode_truncated_vector);
    RUN_TEST_NS(runner, fatpbinary, decode_type_mismatch_string_as_int);
    RUN_TEST_NS(runner, fatpbinary, decode_type_mismatch_int_as_string);
    RUN_TEST_NS(runner, fatpbinary, decode_type_mismatch_double_as_bool);
    RUN_TEST_NS(runner, fatpbinary, decode_empty_buffer);
    RUN_TEST_NS(runner, fatpbinary, decode_impossible_length);
    RUN_TEST_NS(runner, fatpbinary, decode_partial_map);
    RUN_TEST_NS(runner, fatpbinary, decode_nested_truncation);
    RUN_TEST_NS(runner, fatpbinary, decode_huge_length_protection);

    // Fuzz tests
    RUN_TEST_NS(runner, fatpbinary, fuzz_ints);
    RUN_TEST_NS(runner, fatpbinary, fuzz_doubles);
    RUN_TEST_NS(runner, fatpbinary, fuzz_strings);
    RUN_TEST_NS(runner, fatpbinary, fuzz_vector_int);
    RUN_TEST_NS(runner, fatpbinary, fuzz_map_string_int);
    RUN_TEST_NS(runner, fatpbinary, fuzz_nested_structures);

    fatpbinary::benchmark_fatpbinary();

    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

// =============================================================================
// Standalone Entry Point
// =============================================================================
#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_FatPBinary() ? 0 : 1;
}
#endif
