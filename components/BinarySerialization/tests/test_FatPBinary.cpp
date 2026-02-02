/**
 * @file test_FatPBinary.cpp
 * @brief Comprehensive unit tests for FatPBinary.h
 */
/*
FATP_META:
  meta_version: 1
  component: BinarySerialization
  file_role: test
  path: components/BinarySerialization/tests/test_FatPBinary.cpp
  layer: Testing
  namespace: fat_p
  summary: "Unit tests for FatPBinary."
  api_stability: in_work
  related:
    docs_search: "FatPBinary"
    headers:
      - include/fat_p/FatPBinary.h
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

using fat_p::binary_fatp::binaryDecodeFrom;
using fat_p::binary_fatp::binaryEncodeTo;
using fat_p::binary_fatp::BinaryBuffer;
using fat_p::binary_fatp::BinaryError;
using fat_p::binary_fatp::BinaryReader;
using fat_p::binary_fatp::BinaryResult;
using fat_p::binary_fatp::BinaryWriter;

// ============================================================================
// Buffer Alignment
// ============================================================================

FATP_TEST_CASE(buffer_alignment)
{
    BinaryBuffer buf;
    buf.reserve(64);

    const auto addr = reinterpret_cast<std::uintptr_t>(buf.data());
    FATP_ASSERT_TRUE(addr % 64 == 0, "BinaryBuffer must be 64-byte aligned");

    return true;
}

// ============================================================================
// Primitive Roundtrips via Traits
// ============================================================================

FATP_TEST_CASE(int_roundtrip)
{
    const int values[] = {0, 1, -1, 123456, -987654, std::numeric_limits<int>::min(), std::numeric_limits<int>::max()};

    for (int v : values)
    {
        BinaryBuffer buf;
        auto enc = binaryEncodeTo(buf, v);
        FATP_ASSERT_TRUE(enc.has_value(), "Encode int failed");

        auto dec = binaryDecodeFrom<int>(buf);
        FATP_ASSERT_TRUE(dec.has_value(), dec.error().message.c_str());
        FATP_ASSERT_EQ(*dec, v, "int roundtrip");
    }

    return true;
}

FATP_TEST_CASE(uint64_roundtrip)
{
    const std::uint64_t values[] =
        {0ULL, 1ULL, 23ULL, 65535ULL, 123456789ULL, std::numeric_limits<std::uint64_t>::max()};

    for (std::uint64_t v : values)
    {
        BinaryBuffer buf;
        auto enc = binaryEncodeTo(buf, v);
        FATP_ASSERT_TRUE(enc.has_value(), "Encode uint64 failed");

        auto dec = binaryDecodeFrom<std::uint64_t>(buf);
        FATP_ASSERT_TRUE(dec.has_value(), dec.error().message.c_str());
        FATP_ASSERT_EQ(*dec, v, "uint64 roundtrip");
    }

    return true;
}

FATP_TEST_CASE(bool_roundtrip)
{
    {
        BinaryBuffer buf;
        auto enc = binaryEncodeTo(buf, true);
        FATP_ASSERT_TRUE(enc.has_value(), "Encode true failed");

        auto dec = binaryDecodeFrom<bool>(buf);
        FATP_ASSERT_TRUE(dec.has_value() && *dec == true, "true roundtrip");
    }

    {
        BinaryBuffer buf;
        auto enc = binaryEncodeTo(buf, false);
        FATP_ASSERT_TRUE(enc.has_value(), "Encode false failed");

        auto dec = binaryDecodeFrom<bool>(buf);
        FATP_ASSERT_TRUE(dec.has_value() && *dec == false, "false roundtrip");
    }

    return true;
}

FATP_TEST_CASE(float_roundtrip)
{
    const float values[] = {0.0f, -0.0f, 1.5f, -2.75f, 3.14159f};

    for (float v : values)
    {
        BinaryBuffer buf;
        auto enc = binaryEncodeTo(buf, v);
        FATP_ASSERT_TRUE(enc.has_value(), "Encode float failed");

        auto dec = binaryDecodeFrom<float>(buf);
        FATP_ASSERT_TRUE(dec.has_value(), dec.error().message.c_str());
        FATP_ASSERT_EQ(*dec, v, "float roundtrip");
    }

    return true;
}

FATP_TEST_CASE(double_roundtrip)
{
    const double values[] = {0.0, -0.0, 1.5, -2.75, 1e10, -3.14};

    for (double v : values)
    {
        BinaryBuffer buf;
        auto enc = binaryEncodeTo(buf, v);
        FATP_ASSERT_TRUE(enc.has_value(), "Encode double failed");

        auto dec = binaryDecodeFrom<double>(buf);
        FATP_ASSERT_TRUE(dec.has_value(), dec.error().message.c_str());
        FATP_ASSERT_CLOSE_EPS(*dec, v, 1e-12, "double roundtrip");
    }

    return true;
}

FATP_TEST_CASE(string_roundtrip)
{
    const std::string values[] = {"", "a", "hello world", "UTF-8 \xC3\xA9"};

    for (const auto& s : values)
    {
        BinaryBuffer buf;
        auto enc = binaryEncodeTo(buf, s);
        FATP_ASSERT_TRUE(enc.has_value(), "Encode string failed");

        auto dec = binaryDecodeFrom<std::string>(buf);
        FATP_ASSERT_TRUE(dec.has_value(), dec.error().message.c_str());
        FATP_ASSERT_EQ(*dec, s, "string roundtrip");
    }

    return true;
}

// ============================================================================
// Vector Roundtrips
// ============================================================================

FATP_TEST_CASE(vector_int_roundtrip)
{
    std::vector<int> v = {1, 2, 3, 4, 5};

    BinaryBuffer buf;
    auto enc = binaryEncodeTo(buf, v);
    FATP_ASSERT_TRUE(enc.has_value(), "Encode vector failed");

    auto dec = binaryDecodeFrom<std::vector<int>>(buf);
    FATP_ASSERT_TRUE(dec.has_value(), dec.error().message.c_str());
    FATP_ASSERT_EQ(dec->size(), v.size(), "vector size");
    for (std::size_t i = 0; i < v.size(); ++i)
    {
        FATP_ASSERT_EQ((*dec)[i], v[i], "vector element");
    }

    return true;
}

FATP_TEST_CASE(nested_vector_roundtrip)
{
    std::vector<std::vector<int>> v = {{1, 2}, {3, 4, 5}, {}, {6}};

    BinaryBuffer buf;
    auto enc = binaryEncodeTo(buf, v);
    FATP_ASSERT_TRUE(enc.has_value(), "Encode nested vector failed");

    auto dec = binaryDecodeFrom<std::vector<std::vector<int>>>(buf);
    FATP_ASSERT_TRUE(dec.has_value(), dec.error().message.c_str());
    FATP_ASSERT_EQ(dec->size(), v.size(), "outer vector size");
    for (std::size_t i = 0; i < v.size(); ++i)
    {
        FATP_ASSERT_EQ((*dec)[i].size(), v[i].size(), "inner vector size");
    }

    return true;
}

FATP_TEST_CASE(empty_vector_roundtrip)
{
    std::vector<int> v = {};

    BinaryBuffer buf;
    auto enc = binaryEncodeTo(buf, v);
    FATP_ASSERT_TRUE(enc.has_value(), "Encode empty vector failed");

    auto dec = binaryDecodeFrom<std::vector<int>>(buf);
    FATP_ASSERT_TRUE(dec.has_value(), dec.error().message.c_str());
    FATP_ASSERT_TRUE(dec->empty(), "empty vector roundtrip");

    return true;
}

FATP_TEST_CASE(vector_string_roundtrip)
{
    std::vector<std::string> v = {"one", "two", "three", "", "with spaces"};

    BinaryBuffer buf;
    auto enc = binaryEncodeTo(buf, v);
    FATP_ASSERT_TRUE(enc.has_value(), "Encode vector<string> failed");

    auto dec = binaryDecodeFrom<std::vector<std::string>>(buf);
    FATP_ASSERT_TRUE(dec.has_value(), dec.error().message.c_str());
    FATP_ASSERT_EQ(dec->size(), v.size(), "vector<string> size");
    for (std::size_t i = 0; i < v.size(); ++i)
    {
        FATP_ASSERT_EQ((*dec)[i], v[i], "vector<string> element");
    }

    return true;
}

// ============================================================================
// Map Roundtrips
// ============================================================================

FATP_TEST_CASE(map_roundtrip)
{
    std::map<std::string, int> m{{"a", 1}, {"b", 2}, {"c", 3}};

    BinaryBuffer buf;
    auto enc = binaryEncodeTo(buf, m);
    FATP_ASSERT_TRUE(enc.has_value(), "Encode map failed");

    auto dec = binaryDecodeFrom<std::map<std::string, int>>(buf);
    FATP_ASSERT_TRUE(dec.has_value(), dec.error().message.c_str());
    FATP_ASSERT_EQ(dec->size(), m.size(), "map size");
    FATP_ASSERT_EQ(dec->at("a"), 1, "map value a");
    FATP_ASSERT_EQ(dec->at("b"), 2, "map value b");

    return true;
}

FATP_TEST_CASE(nested_map_roundtrip)
{
    std::map<std::string, std::map<int, std::string>> m{{"x", {{1, "a"}, {2, "b"}}}, {"y", {{3, "c"}}}};

    BinaryBuffer buf;
    auto enc = binaryEncodeTo(buf, m);
    FATP_ASSERT_TRUE(enc.has_value(), "Encode nested map failed");

    auto dec = binaryDecodeFrom<std::map<std::string, std::map<int, std::string>>>(buf);
    FATP_ASSERT_TRUE(dec.has_value(), dec.error().message.c_str());
    FATP_ASSERT_EQ(dec->size(), m.size(), "outer map size");

    return true;
}

FATP_TEST_CASE(empty_map_roundtrip)
{
    std::map<std::string, int> m{};

    BinaryBuffer buf;
    auto enc = binaryEncodeTo(buf, m);
    FATP_ASSERT_TRUE(enc.has_value(), "Encode empty map failed");

    auto dec = binaryDecodeFrom<std::map<std::string, int>>(buf);
    FATP_ASSERT_TRUE(dec.has_value(), dec.error().message.c_str());
    FATP_ASSERT_TRUE(dec->empty(), "empty map roundtrip");

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

FATP_TEST_CASE(enum_roundtrip)
{
    const TestEnum values[] = {TestEnum::Alpha, TestEnum::Beta, TestEnum::Gamma};

    for (TestEnum v : values)
    {
        BinaryBuffer buf;
        auto enc = binaryEncodeTo(buf, v);
        FATP_ASSERT_TRUE(enc.has_value(), "Encode enum failed");

        auto dec = binaryDecodeFrom<TestEnum>(buf);
        FATP_ASSERT_TRUE(dec.has_value(), dec.error().message.c_str());
        FATP_ASSERT_TRUE(*dec == v, "enum roundtrip");
    }

    return true;
}

// ============================================================================
// Cross-Container Compatibility
// ============================================================================

FATP_TEST_CASE(cross_container)
{
    BinaryBuffer hbuf;
    auto enc = binaryEncodeTo(hbuf, std::string("test123"));
    FATP_ASSERT_TRUE(enc.has_value(), "Encode failed");

    std::vector<std::uint8_t> sbuf(hbuf.begin(), hbuf.end());

    auto r1 = binaryDecodeFrom<std::string>(hbuf);
    FATP_ASSERT_TRUE(r1.has_value() && *r1 == "test123", "Decode from HpcVector");

    auto r2 = binaryDecodeFrom<std::string>(sbuf);
    FATP_ASSERT_TRUE(r2.has_value() && *r2 == "test123", "Decode from std::vector");

    return true;
}

// ============================================================================
// Malformed Input Tests
// ============================================================================

FATP_TEST_CASE(decode_invalid_tag)
{
    std::vector<std::uint8_t> bad = {0xFFU};

    auto dec = binaryDecodeFrom<int>(bad);
    FATP_ASSERT_TRUE(!dec.has_value(), "Invalid tag should fail");
    FATP_ASSERT_TRUE(!dec.error().message.empty(), "Error message should exist");

    return true;
}

FATP_TEST_CASE(decode_truncated_string)
{
    BinaryBuffer buf;
    BinaryWriter<BinaryBuffer> writer(buf);
    writer.writeString("abcdefghij");

    std::vector<std::uint8_t> truncated(buf.begin(), buf.begin() + 5);

    auto dec = binaryDecodeFrom<std::string>(truncated);
    FATP_ASSERT_TRUE(!dec.has_value(), "Truncated string should fail");

    return true;
}

FATP_TEST_CASE(decode_truncated_vector)
{
    std::vector<int> v = {1, 2, 3, 4, 5};

    BinaryBuffer buf;
    auto enc = binaryEncodeTo(buf, v);
    FATP_ASSERT_TRUE(enc.has_value(), "Encode failed");

    for (std::size_t cut = 1; cut < buf.size(); ++cut)
    {
        std::vector<std::uint8_t> truncated(buf.begin(), buf.begin() + cut);
        auto dec = binaryDecodeFrom<std::vector<int>>(truncated);
        FATP_ASSERT_TRUE(!dec.has_value(), "Truncated vector should fail");
    }

    return true;
}

FATP_TEST_CASE(decode_type_mismatch_string_as_int)
{
    BinaryBuffer buf;
    auto enc = binaryEncodeTo(buf, std::string("not an int"));
    FATP_ASSERT_TRUE(enc.has_value(), "Encode string failed");

    auto dec = binaryDecodeFrom<int>(buf);
    FATP_ASSERT_TRUE(!dec.has_value(), "Type mismatch should fail");

    return true;
}

FATP_TEST_CASE(decode_type_mismatch_int_as_string)
{
    BinaryBuffer buf;
    auto enc = binaryEncodeTo(buf, 42);
    FATP_ASSERT_TRUE(enc.has_value(), "Encode int failed");

    auto dec = binaryDecodeFrom<std::string>(buf);
    FATP_ASSERT_TRUE(!dec.has_value(), "Type mismatch should fail");

    return true;
}

FATP_TEST_CASE(decode_type_mismatch_double_as_bool)
{
    BinaryBuffer buf;
    auto enc = binaryEncodeTo(buf, 3.14);
    FATP_ASSERT_TRUE(enc.has_value(), "Encode double failed");

    auto dec = binaryDecodeFrom<bool>(buf);
    FATP_ASSERT_TRUE(!dec.has_value(), "Type mismatch should fail");

    return true;
}

FATP_TEST_CASE(decode_empty_buffer)
{
    std::vector<std::uint8_t> empty;

    auto dec_int = binaryDecodeFrom<int>(empty);
    FATP_ASSERT_TRUE(!dec_int.has_value(), "Empty buffer should fail for int");

    auto dec_str = binaryDecodeFrom<std::string>(empty);
    FATP_ASSERT_TRUE(!dec_str.has_value(), "Empty buffer should fail for string");

    auto dec_vec = binaryDecodeFrom<std::vector<int>>(empty);
    FATP_ASSERT_TRUE(!dec_vec.has_value(), "Empty buffer should fail for vector");

    return true;
}

FATP_TEST_CASE(decode_impossible_length)
{
    BinaryBuffer buf;
    auto enc = binaryEncodeTo(buf, std::string("abcd"));
    FATP_ASSERT_TRUE(enc.has_value(), "Encode failed");

    std::vector<std::uint8_t> mutated(buf.begin(), buf.end());

    if (mutated.size() >= 9)
    {
        mutated[1] = 0xFFU;
        mutated[2] = 0xFFU;
        mutated[3] = 0x00U;
        mutated[4] = 0x00U;
    }

    auto dec = binaryDecodeFrom<std::string>(mutated);
    FATP_ASSERT_TRUE(!dec.has_value(), "Impossible length should fail");

    return true;
}

FATP_TEST_CASE(decode_partial_map)
{
    std::map<std::string, int> m{{"a", 1}, {"b", 2}};

    BinaryBuffer buf;
    auto enc = binaryEncodeTo(buf, m);
    FATP_ASSERT_TRUE(enc.has_value(), "Encode failed");

    std::size_t cut = buf.size() * 2 / 3;
    std::vector<std::uint8_t> truncated(buf.begin(), buf.begin() + cut);

    auto dec = binaryDecodeFrom<std::map<std::string, int>>(truncated);
    FATP_ASSERT_TRUE(!dec.has_value(), "Partial map should fail");

    return true;
}

FATP_TEST_CASE(decode_nested_truncation)
{
    std::vector<std::vector<int>> nested = {{1, 2}, {3, 4, 5}, {6}};

    BinaryBuffer buf;
    auto enc = binaryEncodeTo(buf, nested);
    FATP_ASSERT_TRUE(enc.has_value(), "Encode failed");

    std::size_t cut = buf.size() / 2;
    std::vector<std::uint8_t> truncated(buf.begin(), buf.begin() + cut);

    auto dec = binaryDecodeFrom<std::vector<std::vector<int>>>(truncated);
    FATP_ASSERT_TRUE(!dec.has_value(), "Truncated nested should fail");

    return true;
}

FATP_TEST_CASE(decode_huge_length_protection)
{
    std::vector<std::uint8_t> buf;

    buf.push_back(static_cast<std::uint8_t>(fat_p::binary::TypeTag::Array));

    std::uint64_t huge_len = 0xFFFFFFFFFFFFFFFFULL;
    const auto* len_bytes = reinterpret_cast<const std::uint8_t*>(&huge_len);
    buf.insert(buf.end(), len_bytes, len_bytes + sizeof(huge_len));

    auto dec = binaryDecodeFrom<std::vector<int>>(buf);
    FATP_ASSERT_TRUE(!dec.has_value(), "Huge length should fail");

    return true;
}

// ============================================================================
// Fuzz Tests
// ============================================================================

FATP_TEST_CASE(fuzz_ints)
{
    std::mt19937_64 rng(0xFA7B1A2C3D4E5F6AULL);
    std::uniform_int_distribution<int> dist(std::numeric_limits<int>::min(), std::numeric_limits<int>::max());

    for (int i = 0; i < 2000; ++i)
    {
        const int v = dist(rng);

        BinaryBuffer buf;
        auto enc = binaryEncodeTo(buf, v);
        FATP_ASSERT_TRUE(enc.has_value(), "Encode int failed");

        auto dec = binaryDecodeFrom<int>(buf);
        FATP_ASSERT_TRUE(dec.has_value(), dec.error().message.c_str());
        FATP_ASSERT_EQ(*dec, v, "fuzz int");
    }

    return true;
}

FATP_TEST_CASE(fuzz_doubles)
{
    std::mt19937_64 rng(0xFA7B1A2C3D4E5F6BULL);
    std::uniform_real_distribution<double> dist(-1e6, 1e6);

    for (int i = 0; i < 2000; ++i)
    {
        const double v = dist(rng);

        BinaryBuffer buf;
        auto enc = binaryEncodeTo(buf, v);
        FATP_ASSERT_TRUE(enc.has_value(), "Encode double failed");

        auto dec = binaryDecodeFrom<double>(buf);
        FATP_ASSERT_TRUE(dec.has_value(), dec.error().message.c_str());
        FATP_ASSERT_CLOSE_EPS(*dec, v, 1e-9, "fuzz double");
    }

    return true;
}

FATP_TEST_CASE(fuzz_strings)
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
        auto enc = binaryEncodeTo(buf, s);
        FATP_ASSERT_TRUE(enc.has_value(), "Encode string failed");

        auto dec = binaryDecodeFrom<std::string>(buf);
        FATP_ASSERT_TRUE(dec.has_value(), dec.error().message.c_str());
        FATP_ASSERT_EQ(*dec, s, "fuzz string");
    }

    return true;
}

FATP_TEST_CASE(fuzz_vector_int)
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
        auto enc = binaryEncodeTo(buf, v);
        FATP_ASSERT_TRUE(enc.has_value(), "Encode vector failed");

        auto dec = binaryDecodeFrom<std::vector<int>>(buf);
        FATP_ASSERT_TRUE(dec.has_value(), dec.error().message.c_str());
        FATP_ASSERT_EQ(dec->size(), v.size(), "fuzz vector size");
        for (std::size_t i = 0; i < v.size(); ++i)
        {
            FATP_ASSERT_EQ((*dec)[i], v[i], "fuzz vector element");
        }
    }

    return true;
}

FATP_TEST_CASE(fuzz_map_string_int)
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
        auto enc = binaryEncodeTo(buf, m);
        FATP_ASSERT_TRUE(enc.has_value(), "Encode map failed");

        auto dec = binaryDecodeFrom<std::map<std::string, int>>(buf);
        FATP_ASSERT_TRUE(dec.has_value(), dec.error().message.c_str());
        FATP_ASSERT_EQ(dec->size(), m.size(), "fuzz map size");
    }

    return true;
}

FATP_TEST_CASE(fuzz_nested_structures)
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
        auto enc = binaryEncodeTo(buf, v);
        FATP_ASSERT_TRUE(enc.has_value(), "Encode nested failed");

        auto dec = binaryDecodeFrom<std::vector<std::map<std::string, int>>>(buf);
        FATP_ASSERT_TRUE(dec.has_value(), dec.error().message.c_str());
        FATP_ASSERT_EQ(dec->size(), v.size(), "fuzz nested outer size");
    }

    return true;
}

// ============================================================================
// Benchmarks
// ============================================================================
} // namespace fat_p::testing::fatpbinary

namespace fat_p::testing
{


void run_benchmarks()
{
    using namespace fat_p::testing;

    auto& out = *get_test_config().output;
    out << "\n" << colors::cyan() << "Sanity Benchmark:" << colors::reset() << "\n";
    out << "  (benchmarks are provided by benchmark executables; unit tests do not run full benchmarks)\n\n";
}

bool test_FatPBinary()
{
    FATP_PRINT_HEADER(FATP BINARY)

    TestRunner runner;

    // Buffer and primitives
    FATP_RUN_TEST_NS(runner, fatpbinary, buffer_alignment);
    FATP_RUN_TEST_NS(runner, fatpbinary, int_roundtrip);
    FATP_RUN_TEST_NS(runner, fatpbinary, uint64_roundtrip);
    FATP_RUN_TEST_NS(runner, fatpbinary, bool_roundtrip);
    FATP_RUN_TEST_NS(runner, fatpbinary, float_roundtrip);
    FATP_RUN_TEST_NS(runner, fatpbinary, double_roundtrip);
    FATP_RUN_TEST_NS(runner, fatpbinary, string_roundtrip);

    // Containers
    FATP_RUN_TEST_NS(runner, fatpbinary, vector_int_roundtrip);
    FATP_RUN_TEST_NS(runner, fatpbinary, nested_vector_roundtrip);
    FATP_RUN_TEST_NS(runner, fatpbinary, empty_vector_roundtrip);
    FATP_RUN_TEST_NS(runner, fatpbinary, vector_string_roundtrip);
    FATP_RUN_TEST_NS(runner, fatpbinary, map_roundtrip);
    FATP_RUN_TEST_NS(runner, fatpbinary, nested_map_roundtrip);
    FATP_RUN_TEST_NS(runner, fatpbinary, empty_map_roundtrip);
    FATP_RUN_TEST_NS(runner, fatpbinary, enum_roundtrip);
    FATP_RUN_TEST_NS(runner, fatpbinary, cross_container);

    // Malformed input
    FATP_RUN_TEST_NS(runner, fatpbinary, decode_invalid_tag);
    FATP_RUN_TEST_NS(runner, fatpbinary, decode_truncated_string);
    FATP_RUN_TEST_NS(runner, fatpbinary, decode_truncated_vector);
    FATP_RUN_TEST_NS(runner, fatpbinary, decode_type_mismatch_string_as_int);
    FATP_RUN_TEST_NS(runner, fatpbinary, decode_type_mismatch_int_as_string);
    FATP_RUN_TEST_NS(runner, fatpbinary, decode_type_mismatch_double_as_bool);
    FATP_RUN_TEST_NS(runner, fatpbinary, decode_empty_buffer);
    FATP_RUN_TEST_NS(runner, fatpbinary, decode_impossible_length);
    FATP_RUN_TEST_NS(runner, fatpbinary, decode_partial_map);
    FATP_RUN_TEST_NS(runner, fatpbinary, decode_nested_truncation);
    FATP_RUN_TEST_NS(runner, fatpbinary, decode_huge_length_protection);

    // Fuzz tests
    FATP_RUN_TEST_NS(runner, fatpbinary, fuzz_ints);
    FATP_RUN_TEST_NS(runner, fatpbinary, fuzz_doubles);
    FATP_RUN_TEST_NS(runner, fatpbinary, fuzz_strings);
    FATP_RUN_TEST_NS(runner, fatpbinary, fuzz_vector_int);
    FATP_RUN_TEST_NS(runner, fatpbinary, fuzz_map_string_int);
    FATP_RUN_TEST_NS(runner, fatpbinary, fuzz_nested_structures);


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
