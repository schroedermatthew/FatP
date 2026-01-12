/**
 * @file test_FatPCbor.cpp
 * @brief Comprehensive unit tests for FatPCbor.h
 */
/*
FATP_META:
  meta_version: 1
  component: FatPCbor
  file_role: test
  path: tests/test_FatPCbor.cpp
  namespace: fat_p
  summary: "Unit tests for FatPCbor."
  related:
    docs_search: "FatPCbor"
    headers:
      - fat_p/FatPCbor.h
      - fat_p/FatPTest.h
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

#include "FatPCbor.h"
#include "FatPTest.h"

namespace fat_p::testing::fatpcbor
{

using fat_p::cbor_fatp::CborBuffer;
using fat_p::cbor_fatp::CborWriter;
using fat_p::cbor_fatp::CborReader;
using fat_p::cbor_fatp::CborResult;
using fat_p::cbor_fatp::CborError;
using fat_p::cbor_fatp::CborTraits;
using fat_p::cbor_fatp::cbor_encode_to;
using fat_p::cbor_fatp::cbor_decode_from;

template <typename T>
static CborResult<T> roundtrip(const T& value)
{
    CborBuffer buf;
    auto enc = cbor_encode_to(buf, value);
    if (!enc)
    {
        return make_unexpected(enc.error());
    }
    return cbor_decode_from<T>(buf);
}

// ============================================================================
// Buffer Alignment
// ============================================================================

FATP_TEST_CASE(buffer_alignment)
{
    CborBuffer buf;
    buf.reserve(64);

    const auto addr = reinterpret_cast<std::uintptr_t>(buf.data());
    FATP_ASSERT_TRUE(addr % 64 == 0, "CborBuffer must be 64-byte aligned");

    return true;
}

// ============================================================================
// Primitive Roundtrips via Traits
// ============================================================================

FATP_TEST_CASE(int_roundtrip)
{
    const int values[] = {0, 1, -1, 123456, -987654,
                          std::numeric_limits<int>::min(),
                          std::numeric_limits<int>::max()};

    for (int v : values)
    {
        auto r = roundtrip(v);
        FATP_ASSERT_TRUE(r.has_value(), r.error().message.c_str());
        FATP_ASSERT_EQ(*r, v, "int roundtrip");
    }

    return true;
}

FATP_TEST_CASE(uint64_roundtrip)
{
    const std::uint64_t values[] = {0ULL, 1ULL, 23ULL, 65535ULL, 123456789ULL,
                                    std::numeric_limits<std::uint64_t>::max()};

    for (std::uint64_t v : values)
    {
        auto r = roundtrip(v);
        FATP_ASSERT_TRUE(r.has_value(), r.error().message.c_str());
        FATP_ASSERT_EQ(*r, v, "uint64 roundtrip");
    }

    return true;
}

FATP_TEST_CASE(bool_roundtrip)
{
    {
        auto r = roundtrip(true);
        FATP_ASSERT_TRUE(r.has_value() && *r == true, "true roundtrip");
    }

    {
        auto r = roundtrip(false);
        FATP_ASSERT_TRUE(r.has_value() && *r == false, "false roundtrip");
    }

    return true;
}

FATP_TEST_CASE(double_roundtrip)
{
    const double values[] = {0.0, -0.0, 1.5, -2.75, 1e10, -3.14159265358979};

    for (double v : values)
    {
        auto r = roundtrip(v);
        FATP_ASSERT_TRUE(r.has_value(), r.error().message.c_str());
        FATP_ASSERT_CLOSE_EPS(*r, v, 1e-12, "double roundtrip");
    }

    return true;
}

FATP_TEST_CASE(string_roundtrip)
{
    const std::string values[] = {"", "a", "hello world", "UTF-8 \xC3\xA9"};

    for (const auto& s : values)
    {
        auto r = roundtrip(s);
        FATP_ASSERT_TRUE(r.has_value(), r.error().message.c_str());
        FATP_ASSERT_EQ(*r, s, "string roundtrip");
    }

    return true;
}

// ============================================================================
// Vector Roundtrips
// ============================================================================

FATP_TEST_CASE(vector_int_roundtrip)
{
    std::vector<int> v = {1, 2, 3, 4, 5};

    auto r = roundtrip(v);
    FATP_ASSERT_TRUE(r.has_value(), r.error().message.c_str());
    FATP_ASSERT_EQ(r->size(), v.size(), "vector size");
    for (std::size_t i = 0; i < v.size(); ++i)
    {
        FATP_ASSERT_EQ((*r)[i], v[i], "vector element");
    }

    return true;
}

FATP_TEST_CASE(nested_vector_roundtrip)
{
    std::vector<std::vector<int>> v = {{1, 2}, {3, 4, 5}, {}, {6}};

    auto r = roundtrip(v);
    FATP_ASSERT_TRUE(r.has_value(), r.error().message.c_str());
    FATP_ASSERT_EQ(r->size(), v.size(), "outer vector size");
    for (std::size_t i = 0; i < v.size(); ++i)
    {
        FATP_ASSERT_EQ((*r)[i].size(), v[i].size(), "inner vector size");
    }

    return true;
}

FATP_TEST_CASE(empty_vector_roundtrip)
{
    std::vector<int> v = {};

    auto r = roundtrip(v);
    FATP_ASSERT_TRUE(r.has_value(), r.error().message.c_str());
    FATP_ASSERT_TRUE(r->empty(), "empty vector roundtrip");

    return true;
}

// ============================================================================
// Map Roundtrips
// ============================================================================

FATP_TEST_CASE(map_roundtrip)
{
    std::map<std::string, int> m{{"a", 1}, {"b", 2}, {"c", 3}};

    auto r = roundtrip(m);
    FATP_ASSERT_TRUE(r.has_value(), r.error().message.c_str());
    FATP_ASSERT_EQ(r->size(), m.size(), "map size");
    FATP_ASSERT_EQ(r->at("a"), 1, "map value a");
    FATP_ASSERT_EQ(r->at("b"), 2, "map value b");

    return true;
}

FATP_TEST_CASE(nested_map_roundtrip)
{
    std::map<std::string, std::map<int, std::string>> m{
        {"x", {{1, "a"}, {2, "b"}}},
        {"y", {{3, "c"}}}
    };

    auto r = roundtrip(m);
    FATP_ASSERT_TRUE(r.has_value(), r.error().message.c_str());
    FATP_ASSERT_EQ(r->size(), m.size(), "outer map size");

    return true;
}

FATP_TEST_CASE(empty_map_roundtrip)
{
    std::map<std::string, int> m{};

    auto r = roundtrip(m);
    FATP_ASSERT_TRUE(r.has_value(), r.error().message.c_str());
    FATP_ASSERT_TRUE(r->empty(), "empty map roundtrip");

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
        auto r = roundtrip(v);
        FATP_ASSERT_TRUE(r.has_value(), r.error().message.c_str());
        FATP_ASSERT_TRUE(*r == v, "enum roundtrip");
    }

    return true;
}

// ============================================================================
// Custom Struct with CborTraits
// ============================================================================

struct Complex
{
    int a = 0;
    std::string b;
    std::vector<double> c;
};

} // namespace fat_p::testing::fatpcbor

namespace fat_p::cbor_fatp
{

template <>
struct CborTraits<::fat_p::testing::fatpcbor::Complex>
{
    template <typename Writer>
    static void encode(Writer& w, const testing::fatpcbor::Complex& x)
    {
        w.write_map_header(3U);
        CborTraits<std::string>::encode(w, std::string("a"));
        CborTraits<int>::encode(w, x.a);
        CborTraits<std::string>::encode(w, std::string("b"));
        CborTraits<std::string>::encode(w, x.b);
        CborTraits<std::string>::encode(w, std::string("c"));
        CborTraits<std::vector<double>>::encode(w, x.c);
    }

    template <typename Reader>
    static CborResult<testing::fatpcbor::Complex> decode(Reader& r)
    {
        auto len = r.read_map_header();
        if (!len)
        {
            return make_unexpected(len.error());
        }

        testing::fatpcbor::Complex out{};
        for (std::uint64_t i = 0; i < *len; ++i)
        {
            auto k = CborTraits<std::string>::decode(r);
            if (!k)
            {
                return make_unexpected(k.error());
            }

            if (*k == "a")
            {
                auto v = CborTraits<int>::decode(r);
                if (!v)
                {
                    return make_unexpected(v.error());
                }
                out.a = *v;
            }
            else if (*k == "b")
            {
                auto v = CborTraits<std::string>::decode(r);
                if (!v)
                {
                    return make_unexpected(v.error());
                }
                out.b = *v;
            }
            else if (*k == "c")
            {
                auto v = CborTraits<std::vector<double>>::decode(r);
                if (!v)
                {
                    return make_unexpected(v.error());
                }
                out.c = std::move(*v);
            }
            else
            {
                return make_unexpected(CborError("Unknown map key"));
            }
        }
        return out;
    }
};

} // namespace fat_p::cbor_fatp

namespace fat_p::testing::fatpcbor
{

FATP_TEST_CASE(complex_struct_roundtrip)
{
    Complex x{42, "hello", {1.1, 2.2, 3.3}};

    auto r = roundtrip(x);
    FATP_ASSERT_TRUE(r.has_value(), r.error().message.c_str());
    FATP_ASSERT_EQ(r->a, x.a, "Complex.a");
    FATP_ASSERT_EQ(r->b, x.b, "Complex.b");
    FATP_ASSERT_EQ(r->c.size(), x.c.size(), "Complex.c size");

    return true;
}

// ============================================================================
// Cross-Container Compatibility
// ============================================================================

FATP_TEST_CASE(cross_container)
{
    CborBuffer hbuf;
    auto enc = cbor_encode_to(hbuf, std::string("test123"));
    FATP_ASSERT_TRUE(enc.has_value(), "Encode failed");

    std::vector<std::uint8_t> sbuf(hbuf.begin(), hbuf.end());

    auto r1 = cbor_decode_from<std::string>(hbuf);
    FATP_ASSERT_TRUE(r1.has_value() && *r1 == "test123", "Decode from HpcVector");

    auto r2 = cbor_decode_from<std::string>(sbuf);
    FATP_ASSERT_TRUE(r2.has_value() && *r2 == "test123", "Decode from std::vector");

    return true;
}

// ============================================================================
// Malformed Input Tests
// ============================================================================

FATP_TEST_CASE(decode_invalid_initial_byte)
{
    std::vector<std::uint8_t> bad = {0xFFU};

    auto r_int = cbor_decode_from<int>(bad);
    FATP_ASSERT_TRUE(!r_int.has_value(), "0xFF should not decode as int");

    auto r_str = cbor_decode_from<std::string>(bad);
    FATP_ASSERT_TRUE(!r_str.has_value(), "0xFF should not decode as string");

    return true;
}

FATP_TEST_CASE(decode_truncated_string)
{
    const std::string s = "abcdef";

    CborBuffer buf;
    auto enc = cbor_encode_to(buf, s);
    FATP_ASSERT_TRUE(enc.has_value(), "Encode failed");

    for (std::size_t cut = 1; cut < buf.size(); ++cut)
    {
        std::vector<std::uint8_t> truncated(buf.begin(), buf.begin() + cut);
        auto r = cbor_decode_from<std::string>(truncated);
        FATP_ASSERT_TRUE(!r.has_value(), "Truncated string should fail");
    }

    return true;
}

FATP_TEST_CASE(decode_truncated_vector)
{
    std::vector<int> v = {1, 2, 3, 4, 5};

    CborBuffer buf;
    auto enc = cbor_encode_to(buf, v);
    FATP_ASSERT_TRUE(enc.has_value(), "Encode failed");

    for (std::size_t cut = 1; cut < buf.size(); ++cut)
    {
        std::vector<std::uint8_t> truncated(buf.begin(), buf.begin() + cut);
        auto r = cbor_decode_from<std::vector<int>>(truncated);
        FATP_ASSERT_TRUE(!r.has_value(), "Truncated vector should fail");
    }

    return true;
}

FATP_TEST_CASE(decode_impossible_length)
{
    const std::string s = "abcd";

    CborBuffer buf;
    auto enc = cbor_encode_to(buf, s);
    FATP_ASSERT_TRUE(enc.has_value(), "Encode failed");

    std::vector<std::uint8_t> mutated(buf.begin(), buf.end());
    mutated[0] = 0x6AU;

    auto r = cbor_decode_from<std::string>(mutated);
    FATP_ASSERT_TRUE(!r.has_value(), "Impossible length should fail");

    return true;
}

FATP_TEST_CASE(decode_type_mismatch_string_as_int)
{
    CborBuffer buf;
    auto enc = cbor_encode_to(buf, std::string("not an int"));
    FATP_ASSERT_TRUE(enc.has_value(), "Encode string failed");

    auto r = cbor_decode_from<int>(buf);
    FATP_ASSERT_TRUE(!r.has_value(), "Type mismatch should fail");

    return true;
}

FATP_TEST_CASE(decode_type_mismatch_int_as_string)
{
    CborBuffer buf;
    auto enc = cbor_encode_to(buf, 42);
    FATP_ASSERT_TRUE(enc.has_value(), "Encode int failed");

    auto r = cbor_decode_from<std::string>(buf);
    FATP_ASSERT_TRUE(!r.has_value(), "Type mismatch should fail");

    return true;
}

FATP_TEST_CASE(decode_type_mismatch_double_as_bool)
{
    CborBuffer buf;
    auto enc = cbor_encode_to(buf, 3.14);
    FATP_ASSERT_TRUE(enc.has_value(), "Encode double failed");

    auto r = cbor_decode_from<bool>(buf);
    FATP_ASSERT_TRUE(!r.has_value(), "Type mismatch should fail");

    return true;
}

FATP_TEST_CASE(decode_empty_buffer)
{
    std::vector<std::uint8_t> empty;

    auto r_int = cbor_decode_from<int>(empty);
    FATP_ASSERT_TRUE(!r_int.has_value(), "Empty buffer should fail for int");

    auto r_str = cbor_decode_from<std::string>(empty);
    FATP_ASSERT_TRUE(!r_str.has_value(), "Empty buffer should fail for string");

    auto r_vec = cbor_decode_from<std::vector<int>>(empty);
    FATP_ASSERT_TRUE(!r_vec.has_value(), "Empty buffer should fail for vector");

    return true;
}

FATP_TEST_CASE(decode_partial_map)
{
    std::map<std::string, int> m{{"a", 1}, {"b", 2}};

    CborBuffer buf;
    auto enc = cbor_encode_to(buf, m);
    FATP_ASSERT_TRUE(enc.has_value(), "Encode failed");

    std::size_t cut = buf.size() * 2 / 3;
    std::vector<std::uint8_t> truncated(buf.begin(), buf.begin() + cut);

    auto r = cbor_decode_from<std::map<std::string, int>>(truncated);
    FATP_ASSERT_TRUE(!r.has_value(), "Partial map should fail");

    return true;
}

FATP_TEST_CASE(decode_nested_truncation)
{
    std::vector<std::vector<int>> nested = {{1, 2}, {3, 4, 5}, {6}};

    CborBuffer buf;
    auto enc = cbor_encode_to(buf, nested);
    FATP_ASSERT_TRUE(enc.has_value(), "Encode failed");

    std::size_t cut = buf.size() / 2;
    std::vector<std::uint8_t> truncated(buf.begin(), buf.begin() + cut);

    auto r = cbor_decode_from<std::vector<std::vector<int>>>(truncated);
    FATP_ASSERT_TRUE(!r.has_value(), "Truncated nested should fail");

    return true;
}

FATP_TEST_CASE(decode_trailing_garbage)
{
    const int value = 42;

    CborBuffer buf;
    auto enc = cbor_encode_to(buf, value);
    FATP_ASSERT_TRUE(enc.has_value(), "Encode failed");

    std::vector<std::uint8_t> mutated(buf.begin(), buf.end());
    mutated.push_back(0xFFU);
    mutated.push_back(0xFFU);

    auto r = cbor_decode_from<int>(mutated);
    FATP_ASSERT_TRUE(r.has_value(), "Decode first value should succeed");
    FATP_ASSERT_EQ(*r, value, "Decoded value");

    CborReader reader(mutated);
    auto v1 = reader.read_int();
    FATP_ASSERT_TRUE(v1.has_value(), "First read should succeed");
    auto v2 = reader.read_int();
    FATP_ASSERT_TRUE(!v2.has_value(), "Second read should fail on garbage");

    return true;
}

// ============================================================================
// Fuzz Tests
// ============================================================================

FATP_TEST_CASE(fuzz_ints)
{
    std::mt19937_64 rng(0xFACB0A1C3D4E5F6AULL);
    std::uniform_int_distribution<int> dist(
        std::numeric_limits<int>::min(),
        std::numeric_limits<int>::max());

    for (int i = 0; i < 2000; ++i)
    {
        const int v = dist(rng);

        auto r = roundtrip(v);
        FATP_ASSERT_TRUE(r.has_value(), r.error().message.c_str());
        FATP_ASSERT_EQ(*r, v, "fuzz int");
    }

    return true;
}

FATP_TEST_CASE(fuzz_doubles)
{
    std::mt19937_64 rng(0xFACB0A1C3D4E5F6BULL);
    std::uniform_real_distribution<double> dist(-1e6, 1e6);

    for (int i = 0; i < 2000; ++i)
    {
        const double v = dist(rng);

        auto r = roundtrip(v);
        FATP_ASSERT_TRUE(r.has_value(), r.error().message.c_str());
        FATP_ASSERT_CLOSE_EPS(*r, v, 1e-9, "fuzz double");
    }

    return true;
}

FATP_TEST_CASE(fuzz_strings)
{
    std::mt19937_64 rng(0xFACB0A1C3D4E5F6CULL);
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

        auto r = roundtrip(s);
        FATP_ASSERT_TRUE(r.has_value(), r.error().message.c_str());
        FATP_ASSERT_EQ(*r, s, "fuzz string");
    }

    return true;
}

FATP_TEST_CASE(fuzz_vector_int)
{
    std::mt19937_64 rng(0xFACB0A1C3D4E5F6DULL);
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

        auto r = roundtrip(v);
        FATP_ASSERT_TRUE(r.has_value(), r.error().message.c_str());
        FATP_ASSERT_EQ(r->size(), v.size(), "fuzz vector size");
        for (std::size_t i = 0; i < v.size(); ++i)
        {
            FATP_ASSERT_EQ((*r)[i], v[i], "fuzz vector element");
        }
    }

    return true;
}

FATP_TEST_CASE(fuzz_map_string_int)
{
    std::mt19937_64 rng(0xFACB0A1C3D4E5F6EULL);
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

        auto r = roundtrip(m);
        FATP_ASSERT_TRUE(r.has_value(), r.error().message.c_str());
        FATP_ASSERT_EQ(r->size(), m.size(), "fuzz map size");
    }

    return true;
}

FATP_TEST_CASE(fuzz_nested_structures)
{
    std::mt19937_64 rng(0xFACB0A1C3D4E5F6FULL);
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

        auto r = roundtrip(v);
        FATP_ASSERT_TRUE(r.has_value(), r.error().message.c_str());
        FATP_ASSERT_EQ(r->size(), v.size(), "fuzz nested outer size");
    }

    return true;
}

// ============================================================================
// Benchmarks
// ============================================================================

void benchmark_fatpcbor()
{
    std::cout << "\n" << colors::cyan() << "FatPCbor Benchmarks:" << colors::reset() << "\n\n";

    std::vector<int> vec;
    vec.reserve(10000);
    for (int i = 0; i < 10000; ++i)
    {
        vec.push_back(i);
    }

    double enc_time = measure_perf([&vec]()
    {
        CborBuffer buf;
        auto rc = cbor_encode_to(buf, vec);
        DoNotOptimize(buf.data());
        (void)rc;
    }, 1000, 100);
    std::cout << "Encode vector<int> (10000): " << format_time(enc_time) << "\n";

    CborBuffer vec_buf;
    (void)cbor_encode_to(vec_buf, vec);

    double dec_time = measure_perf([&vec_buf]()
    {
        auto rc = cbor_decode_from<std::vector<int>>(vec_buf);
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
        CborBuffer buf;
        auto rc = cbor_encode_to(buf, m);
        DoNotOptimize(buf.data());
        (void)rc;
    }, 500, 50);
    std::cout << "Encode map<string,int> (1000): " << format_time(map_enc_time) << "\n";

    CborBuffer map_buf;
    (void)cbor_encode_to(map_buf, m);

    double map_dec_time = measure_perf([&map_buf]()
    {
        auto rc = cbor_decode_from<std::map<std::string, int>>(map_buf);
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
        CborBuffer buf;
        auto rc = cbor_encode_to(buf, nested);
        DoNotOptimize(buf.data());
        (void)rc;
    }, 100, 10);
    std::cout << "Encode nested (64x64): " << format_time(nested_enc_time) << "\n";

    CborBuffer nested_buf;
    (void)cbor_encode_to(nested_buf, nested);

    double nested_dec_time = measure_perf([&nested_buf]()
    {
        auto rc =
            cbor_decode_from<std::vector<std::map<std::string, int>>>(nested_buf);
        DoNotOptimize(rc);
        (void)rc;
    }, 100, 10);
    std::cout << "Decode nested (64x64): " << format_time(nested_dec_time) << "\n";
    std::cout << "Encoded size: " << nested_buf.size() << " bytes\n";
}

} // namespace fat_p::testing::fatpcbor

namespace fat_p::testing
{

bool test_FatPCbor()
{
    FATP_PRINT_HEADER(FATP CBOR)

    TestRunner runner;

    // Buffer and primitives
    FATP_RUN_TEST_NS(runner, fatpcbor, buffer_alignment);
    FATP_RUN_TEST_NS(runner, fatpcbor, int_roundtrip);
    FATP_RUN_TEST_NS(runner, fatpcbor, uint64_roundtrip);
    FATP_RUN_TEST_NS(runner, fatpcbor, bool_roundtrip);
    FATP_RUN_TEST_NS(runner, fatpcbor, double_roundtrip);
    FATP_RUN_TEST_NS(runner, fatpcbor, string_roundtrip);

    // Containers
    FATP_RUN_TEST_NS(runner, fatpcbor, vector_int_roundtrip);
    FATP_RUN_TEST_NS(runner, fatpcbor, nested_vector_roundtrip);
    FATP_RUN_TEST_NS(runner, fatpcbor, empty_vector_roundtrip);
    FATP_RUN_TEST_NS(runner, fatpcbor, map_roundtrip);
    FATP_RUN_TEST_NS(runner, fatpcbor, nested_map_roundtrip);
    FATP_RUN_TEST_NS(runner, fatpcbor, empty_map_roundtrip);
    FATP_RUN_TEST_NS(runner, fatpcbor, enum_roundtrip);
    FATP_RUN_TEST_NS(runner, fatpcbor, complex_struct_roundtrip);
    FATP_RUN_TEST_NS(runner, fatpcbor, cross_container);

    // Malformed input
    FATP_RUN_TEST_NS(runner, fatpcbor, decode_invalid_initial_byte);
    FATP_RUN_TEST_NS(runner, fatpcbor, decode_truncated_string);
    FATP_RUN_TEST_NS(runner, fatpcbor, decode_truncated_vector);
    FATP_RUN_TEST_NS(runner, fatpcbor, decode_impossible_length);
    FATP_RUN_TEST_NS(runner, fatpcbor, decode_type_mismatch_string_as_int);
    FATP_RUN_TEST_NS(runner, fatpcbor, decode_type_mismatch_int_as_string);
    FATP_RUN_TEST_NS(runner, fatpcbor, decode_type_mismatch_double_as_bool);
    FATP_RUN_TEST_NS(runner, fatpcbor, decode_empty_buffer);
    FATP_RUN_TEST_NS(runner, fatpcbor, decode_partial_map);
    FATP_RUN_TEST_NS(runner, fatpcbor, decode_nested_truncation);
    FATP_RUN_TEST_NS(runner, fatpcbor, decode_trailing_garbage);

    // Fuzz tests
    FATP_RUN_TEST_NS(runner, fatpcbor, fuzz_ints);
    FATP_RUN_TEST_NS(runner, fatpcbor, fuzz_doubles);
    FATP_RUN_TEST_NS(runner, fatpcbor, fuzz_strings);
    FATP_RUN_TEST_NS(runner, fatpcbor, fuzz_vector_int);
    FATP_RUN_TEST_NS(runner, fatpcbor, fuzz_map_string_int);
    FATP_RUN_TEST_NS(runner, fatpcbor, fuzz_nested_structures);

    fatpcbor::benchmark_fatpcbor();

    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

// =============================================================================
// Standalone Entry Point
// =============================================================================
#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_FatPCbor() ? 0 : 1;
}
#endif
