/**
 * @file test_CborStreamLite.cpp
 * @brief Comprehensive unit tests for CborStreamLite.h
 */
/*
FATP_META:
  meta_version: 1
  component: Cbor
  file_role: test
  path: components/Cbor/tests/test_CborStreamLite.cpp
  layer: Testing
  namespace: fat_p
  summary: "Unit tests for CborStreamLite."
  api_stability: in_work
  related:
    docs_search: "CborStreamLite"
    headers:
      - include/fat_p/CborStreamLite.h
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

#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "CborStreamLite.h"
#include "FatPTest.h"

namespace fat_p::testing::cborstreamlite
{

using namespace fat_p::cbor_stream;

// =============================================================================
// Helper: Encode CBOR manually for test cases
// =============================================================================

inline void write_type_arg(std::vector<uint8_t>& buf, uint8_t major, uint64_t arg)
{
    uint8_t mt = major << 5;
    if (arg <= 23)
    {
        buf.push_back(mt | static_cast<uint8_t>(arg));
    }
    else if (arg <= 0xFF)
    {
        buf.push_back(mt | 24);
        buf.push_back(static_cast<uint8_t>(arg));
    }
    else if (arg <= 0xFFFF)
    {
        buf.push_back(mt | 25);
        buf.push_back(static_cast<uint8_t>(arg >> 8));
        buf.push_back(static_cast<uint8_t>(arg));
    }
    else if (arg <= 0xFFFFFFFF)
    {
        buf.push_back(mt | 26);
        buf.push_back(static_cast<uint8_t>(arg >> 24));
        buf.push_back(static_cast<uint8_t>(arg >> 16));
        buf.push_back(static_cast<uint8_t>(arg >> 8));
        buf.push_back(static_cast<uint8_t>(arg));
    }
    else
    {
        buf.push_back(mt | 27);
        for (int i = 7; i >= 0; --i)
        {
            buf.push_back(static_cast<uint8_t>(arg >> (i * 8)));
        }
    }
}

inline std::vector<uint8_t> encode_uint(uint64_t v)
{
    std::vector<uint8_t> buf;
    write_type_arg(buf, 0, v);
    return buf;
}

inline std::vector<uint8_t> encode_negint(uint64_t v)
{
    std::vector<uint8_t> buf;
    write_type_arg(buf, 1, v);
    return buf;
}

inline std::vector<uint8_t> encode_bytes(const std::vector<uint8_t>& b)
{
    std::vector<uint8_t> buf;
    write_type_arg(buf, 2, b.size());
    buf.insert(buf.end(), b.begin(), b.end());
    return buf;
}

inline std::vector<uint8_t> encode_text(const std::string& s)
{
    std::vector<uint8_t> buf;
    write_type_arg(buf, 3, s.size());
    buf.insert(buf.end(), s.begin(), s.end());
    return buf;
}

inline std::vector<uint8_t> encode_array_header(size_t n)
{
    std::vector<uint8_t> buf;
    write_type_arg(buf, 4, n);
    return buf;
}

inline std::vector<uint8_t> encode_map_header(size_t n)
{
    std::vector<uint8_t> buf;
    write_type_arg(buf, 5, n);
    return buf;
}

inline std::vector<uint8_t> encode_false()
{
    return {0xF4};
}

inline std::vector<uint8_t> encode_true()
{
    return {0xF5};
}

inline std::vector<uint8_t> encode_null()
{
    return {0xF6};
}

// =============================================================================
// Basic Type Tests
// =============================================================================

FATP_TEST_CASE(parse_unsigned_integers)
{
    CborStreamParser parser;

    // Tiny integer (0-23)
    auto data = encode_uint(0);
    FATP_ASSERT_TRUE(parser.feed(data) == ParseStatus::Done, "Parse uint 0");
    FATP_ASSERT_TRUE(parser.result().is_unsigned(), "Is unsigned");
    FATP_ASSERT_TRUE(parser.result().as_unsigned() == 0, "Value 0");

    parser.reset();
    data = encode_uint(23);
    FATP_ASSERT_TRUE(parser.feed(data) == ParseStatus::Done, "Parse uint 23");
    FATP_ASSERT_TRUE(parser.result().as_unsigned() == 23, "Value 23");

    // 1-byte integer
    parser.reset();
    data = encode_uint(100);
    FATP_ASSERT_TRUE(parser.feed(data) == ParseStatus::Done, "Parse uint 100");
    FATP_ASSERT_TRUE(parser.result().as_unsigned() == 100, "Value 100");

    // 2-byte integer
    parser.reset();
    data = encode_uint(1000);
    FATP_ASSERT_TRUE(parser.feed(data) == ParseStatus::Done, "Parse uint 1000");
    FATP_ASSERT_TRUE(parser.result().as_unsigned() == 1000, "Value 1000");

    // 4-byte integer
    parser.reset();
    data = encode_uint(100000);
    FATP_ASSERT_TRUE(parser.feed(data) == ParseStatus::Done, "Parse uint 100000");
    FATP_ASSERT_TRUE(parser.result().as_unsigned() == 100000, "Value 100000");

    // 8-byte integer
    parser.reset();
    data = encode_uint(0x123456789ABCDEFULL);
    FATP_ASSERT_TRUE(parser.feed(data) == ParseStatus::Done, "Parse uint large");
    FATP_ASSERT_TRUE(parser.result().as_unsigned() == 0x123456789ABCDEFULL, "Value large");

    return true;
}

FATP_TEST_CASE(parse_negative_integers)
{
    CborStreamParser parser;

    // -1 is encoded as negint(0)
    auto data = encode_negint(0);
    FATP_ASSERT_TRUE(parser.feed(data) == ParseStatus::Done, "Parse -1");
    FATP_ASSERT_TRUE(parser.result().is_signed(), "Is signed");
    FATP_ASSERT_TRUE(parser.result().as_signed() == -1, "Value -1");

    // -100 is encoded as negint(99)
    parser.reset();
    data = encode_negint(99);
    FATP_ASSERT_TRUE(parser.feed(data) == ParseStatus::Done, "Parse -100");
    FATP_ASSERT_TRUE(parser.result().as_signed() == -100, "Value -100");

    // -1000 is encoded as negint(999)
    parser.reset();
    data = encode_negint(999);
    FATP_ASSERT_TRUE(parser.feed(data) == ParseStatus::Done, "Parse -1000");
    FATP_ASSERT_TRUE(parser.result().as_signed() == -1000, "Value -1000");

    return true;
}

FATP_TEST_CASE(parse_byte_strings)
{
    CborStreamParser parser;

    // Empty byte string
    auto data = encode_bytes({});
    FATP_ASSERT_TRUE(parser.feed(data) == ParseStatus::Done, "Parse empty bytes");
    FATP_ASSERT_TRUE(parser.result().is_bytes(), "Is bytes");
    FATP_ASSERT_TRUE(parser.result().as_bytes().empty(), "Empty bytes");

    // Short byte string
    parser.reset();
    data = encode_bytes({0x01, 0x02, 0x03, 0x04});
    FATP_ASSERT_TRUE(parser.feed(data) == ParseStatus::Done, "Parse bytes");
    auto bytes = parser.result().as_bytes();
    FATP_ASSERT_TRUE(bytes.size() == 4, "Bytes size");
    FATP_ASSERT_TRUE(bytes[0] == 1 && bytes[3] == 4, "Bytes content");

    return true;
}

FATP_TEST_CASE(parse_text_strings)
{
    CborStreamParser parser;

    // Empty string
    auto data = encode_text("");
    FATP_ASSERT_TRUE(parser.feed(data) == ParseStatus::Done, "Parse empty text");
    FATP_ASSERT_TRUE(parser.result().is_string(), "Is string");
    FATP_ASSERT_TRUE(parser.result().as_string().empty(), "Empty string");

    // Regular string
    parser.reset();
    data = encode_text("hello");
    FATP_ASSERT_TRUE(parser.feed(data) == ParseStatus::Done, "Parse hello");
    FATP_ASSERT_TRUE(parser.result().as_string() == "hello", "String content");

    // Longer string
    parser.reset();
    std::string longstr(300, 'x');
    data = encode_text(longstr);
    FATP_ASSERT_TRUE(parser.feed(data) == ParseStatus::Done, "Parse long string");
    FATP_ASSERT_TRUE(parser.result().as_string() == longstr, "Long string content");

    return true;
}

FATP_TEST_CASE(parse_simple_values)
{
    CborStreamParser parser;

    // false
    FATP_ASSERT_TRUE(parser.feed(encode_false()) == ParseStatus::Done, "Parse false");
    FATP_ASSERT_TRUE(parser.result().is_bool(), "Is bool");
    FATP_ASSERT_TRUE(parser.result().as_bool() == false, "Value false");

    // true
    parser.reset();
    FATP_ASSERT_TRUE(parser.feed(encode_true()) == ParseStatus::Done, "Parse true");
    FATP_ASSERT_TRUE(parser.result().as_bool() == true, "Value true");

    // null
    parser.reset();
    FATP_ASSERT_TRUE(parser.feed(encode_null()) == ParseStatus::Done, "Parse null");
    FATP_ASSERT_TRUE(parser.result().is_null(), "Is null");

    return true;
}

FATP_TEST_CASE(parse_floats)
{
    CborStreamParser parser;

    // Double precision (0xFB prefix)
    std::vector<uint8_t> data = {0xFB};
    double pi = 3.141592653589793;
    uint64_t bits;
    std::memcpy(&bits, &pi, sizeof(bits));
    for (int i = 7; i >= 0; --i)
    {
        data.push_back(static_cast<uint8_t>(bits >> (i * 8)));
    }

    FATP_ASSERT_TRUE(parser.feed(data) == ParseStatus::Done, "Parse double");
    FATP_ASSERT_TRUE(parser.result().is_float(), "Is float");
    FATP_ASSERT_TRUE(std::fabs(parser.result().as_float() - pi) < 1e-15, "Double value");

    return true;
}

// =============================================================================
// Container Tests
// =============================================================================

FATP_TEST_CASE(parse_arrays)
{
    CborStreamParser parser;

    // Empty array
    auto data = encode_array_header(0);
    FATP_ASSERT_TRUE(parser.feed(data) == ParseStatus::Done, "Parse empty array");
    FATP_ASSERT_TRUE(parser.result().is_array(), "Is array");
    FATP_ASSERT_TRUE(parser.result().as_array().empty(), "Empty array");

    // Array of integers [1, 2, 3]
    parser.reset();
    data = encode_array_header(3);
    auto d1 = encode_uint(1);
    auto d2 = encode_uint(2);
    auto d3 = encode_uint(3);
    data.insert(data.end(), d1.begin(), d1.end());
    data.insert(data.end(), d2.begin(), d2.end());
    data.insert(data.end(), d3.begin(), d3.end());

    FATP_ASSERT_TRUE(parser.feed(data) == ParseStatus::Done, "Parse array [1,2,3]");
    auto& arr = parser.result().as_array();
    FATP_ASSERT_TRUE(arr.size() == 3, "Array size");
    FATP_ASSERT_TRUE(arr[0].as_unsigned() == 1, "arr[0]");
    FATP_ASSERT_TRUE(arr[1].as_unsigned() == 2, "arr[1]");
    FATP_ASSERT_TRUE(arr[2].as_unsigned() == 3, "arr[2]");

    return true;
}

FATP_TEST_CASE(parse_maps)
{
    CborStreamParser parser;

    // Empty map
    auto data = encode_map_header(0);
    FATP_ASSERT_TRUE(parser.feed(data) == ParseStatus::Done, "Parse empty map");
    FATP_ASSERT_TRUE(parser.result().is_map(), "Is map");
    FATP_ASSERT_TRUE(parser.result().as_map().empty(), "Empty map");

    // Map {"a": 1, "b": 2}
    parser.reset();
    data = encode_map_header(2);
    auto ka = encode_text("a");
    auto v1 = encode_uint(1);
    auto kb = encode_text("b");
    auto v2 = encode_uint(2);
    data.insert(data.end(), ka.begin(), ka.end());
    data.insert(data.end(), v1.begin(), v1.end());
    data.insert(data.end(), kb.begin(), kb.end());
    data.insert(data.end(), v2.begin(), v2.end());

    FATP_ASSERT_TRUE(parser.feed(data) == ParseStatus::Done, "Parse map");
    auto& m = parser.result().as_map();
    FATP_ASSERT_TRUE(m.size() == 2, "Map size");

    return true;
}

FATP_TEST_CASE(parse_nested)
{
    CborStreamParser parser;

    // [[1, 2], [3, 4]]
    auto data = encode_array_header(2);

    // First inner array [1, 2]
    auto inner1 = encode_array_header(2);
    auto i1v1 = encode_uint(1);
    auto i1v2 = encode_uint(2);
    inner1.insert(inner1.end(), i1v1.begin(), i1v1.end());
    inner1.insert(inner1.end(), i1v2.begin(), i1v2.end());

    // Second inner array [3, 4]
    auto inner2 = encode_array_header(2);
    auto i2v1 = encode_uint(3);
    auto i2v2 = encode_uint(4);
    inner2.insert(inner2.end(), i2v1.begin(), i2v1.end());
    inner2.insert(inner2.end(), i2v2.begin(), i2v2.end());

    data.insert(data.end(), inner1.begin(), inner1.end());
    data.insert(data.end(), inner2.begin(), inner2.end());

    FATP_ASSERT_TRUE(parser.feed(data) == ParseStatus::Done, "Parse nested");
    auto& outer = parser.result().as_array();
    FATP_ASSERT_TRUE(outer.size() == 2, "Outer size");
    FATP_ASSERT_TRUE(outer[0].is_array(), "Inner 0 is array");
    FATP_ASSERT_TRUE(outer[1].is_array(), "Inner 1 is array");
    FATP_ASSERT_TRUE(outer[0].as_array()[0].as_unsigned() == 1, "[[1,_],[_,_]]");
    FATP_ASSERT_TRUE(outer[1].as_array()[1].as_unsigned() == 4, "[[_,_],[_,4]]");

    return true;
}

// =============================================================================
// Chunked Input Tests (the key streaming feature)
// =============================================================================

FATP_TEST_CASE(chunked_input)
{
    CborStreamParser parser;

    // Build a complex message
    std::vector<uint8_t> data = encode_array_header(3);
    auto s1 = encode_text("hello");
    auto n1 = encode_uint(12345);
    auto b1 = encode_bytes({0xDE, 0xAD, 0xBE, 0xEF});
    data.insert(data.end(), s1.begin(), s1.end());
    data.insert(data.end(), n1.begin(), n1.end());
    data.insert(data.end(), b1.begin(), b1.end());

    // Feed one byte at a time
    for (size_t i = 0; i < data.size() - 1; ++i)
    {
        auto status = parser.feed(&data[i], 1);
        FATP_ASSERT_TRUE(status == ParseStatus::NeedMoreData, "Need more data");
    }

    // Last byte completes parsing
    auto status = parser.feed(&data.back(), 1);
    FATP_ASSERT_TRUE(status == ParseStatus::Done, "Done on last byte");

    // Verify result
    auto& arr = parser.result().as_array();
    FATP_ASSERT_TRUE(arr.size() == 3, "Array size");
    FATP_ASSERT_TRUE(arr[0].as_string() == "hello", "First element");
    FATP_ASSERT_TRUE(arr[1].as_unsigned() == 12345, "Second element");
    FATP_ASSERT_TRUE(arr[2].as_bytes().size() == 4, "Third element size");

    return true;
}

FATP_TEST_CASE(chunked_random_sizes)
{
    CborStreamParser parser;

    // Build test data
    std::vector<uint8_t> data = encode_map_header(2);
    auto k1 = encode_text("key1");
    auto v1 = encode_text("value1");
    auto k2 = encode_text("key2");
    auto v2 = encode_uint(42);
    data.insert(data.end(), k1.begin(), k1.end());
    data.insert(data.end(), v1.begin(), v1.end());
    data.insert(data.end(), k2.begin(), k2.end());
    data.insert(data.end(), v2.begin(), v2.end());

    // Feed in random-sized chunks
    size_t pos = 0;
    size_t chunk_sizes[] = {1, 3, 2, 5, 1, 4, 100};
    size_t chunk_idx = 0;

    while (pos < data.size())
    {
        size_t chunk_size = std::min(chunk_sizes[chunk_idx % 7], data.size() - pos);
        auto status = parser.feed(&data[pos], chunk_size);

        if (status == ParseStatus::Done)
        {
            FATP_ASSERT_TRUE(pos + chunk_size == data.size(), "Done at end");
            break;
        }
        FATP_ASSERT_TRUE(status == ParseStatus::NeedMoreData, "Need more");

        pos += chunk_size;
        ++chunk_idx;
    }

    FATP_ASSERT_TRUE(parser.is_done(), "Parser done");
    FATP_ASSERT_TRUE(parser.result().is_map(), "Result is map");

    return true;
}

// =============================================================================
// Limit Tests
// =============================================================================

FATP_TEST_CASE(limit_max_depth)
{
    CborStreamParser::Limits limits;
    limits.max_depth = 3;
    CborStreamParser parser(limits);

    // Create deeply nested array [[[[1]]]]
    std::vector<uint8_t> data;
    for (int i = 0; i < 5; ++i)
    {
        auto hdr = encode_array_header(1);
        data.insert(data.end(), hdr.begin(), hdr.end());
    }
    auto val = encode_uint(1);
    data.insert(data.end(), val.begin(), val.end());

    auto status = parser.feed(data);
    FATP_ASSERT_TRUE(status == ParseStatus::Error, "Depth exceeded");
    FATP_ASSERT_TRUE(parser.error() == ParseError::MaxDepthExceeded, "Correct error");

    return true;
}

FATP_TEST_CASE(limit_string_size)
{
    CborStreamParser::Limits limits;
    limits.max_string_bytes = 10;
    CborStreamParser parser(limits);

    // Try to parse a 100-byte string
    auto data = encode_text(std::string(100, 'x'));

    auto status = parser.feed(data);
    FATP_ASSERT_TRUE(status == ParseStatus::Error, "String too large");
    FATP_ASSERT_TRUE(parser.error() == ParseError::MaxStringSizeExceeded, "Correct error");

    return true;
}

FATP_TEST_CASE(limit_total_size)
{
    CborStreamParser::Limits limits;
    limits.max_total_bytes = 20;
    CborStreamParser parser(limits);

    // Build data larger than 20 bytes
    auto data = encode_text(std::string(25, 'a'));

    auto status = parser.feed(data);
    FATP_ASSERT_TRUE(status == ParseStatus::Error, "Total size exceeded");
    FATP_ASSERT_TRUE(parser.error() == ParseError::MaxTotalSizeExceeded, "Correct error");

    return true;
}

// =============================================================================
// Error Handling Tests
// =============================================================================

FATP_TEST_CASE(error_truncated_input)
{
    CborStreamParser parser;

    // Start of a 2-byte integer, but only provide 1 byte
    std::vector<uint8_t> data = {0x19, 0x01};

    auto status = parser.feed(data);
    FATP_ASSERT_TRUE(status == ParseStatus::NeedMoreData, "Waiting for more");
    FATP_ASSERT_TRUE(!parser.is_done(), "Not done");

    return true;
}

FATP_TEST_CASE(error_reserved_ai)
{
    CborStreamParser parser;

    // AI 28, 29, 30 are reserved
    std::vector<uint8_t> data = {0x1C};

    auto status = parser.feed(data);
    FATP_ASSERT_TRUE(status == ParseStatus::Error, "Reserved AI");
    FATP_ASSERT_TRUE(parser.error() == ParseError::ReservedAdditionalInfo, "Correct error");

    return true;
}

FATP_TEST_CASE(error_indefinite_length)
{
    CborStreamParser parser;

    // Indefinite-length array (not supported)
    std::vector<uint8_t> data = {0x9F};

    auto status = parser.feed(data);
    FATP_ASSERT_TRUE(status == ParseStatus::Error, "Indefinite not supported");
    FATP_ASSERT_TRUE(parser.error() == ParseError::IndefiniteLengthNotSupported, "Correct error");

    return true;
}

// =============================================================================
// Statistics Tests
// =============================================================================

FATP_TEST_CASE(statistics)
{
    CborStreamParser parser;

    // Parse [[1, 2], 3]
    auto data = encode_array_header(2);
    auto inner = encode_array_header(2);
    auto v1 = encode_uint(1);
    auto v2 = encode_uint(2);
    inner.insert(inner.end(), v1.begin(), v1.end());
    inner.insert(inner.end(), v2.begin(), v2.end());
    auto v3 = encode_uint(3);
    data.insert(data.end(), inner.begin(), inner.end());
    data.insert(data.end(), v3.begin(), v3.end());

    parser.feed(data);

    auto stats = parser.stats();
    FATP_ASSERT_TRUE(stats.bytes_consumed == data.size(), "Bytes consumed");
    FATP_ASSERT_TRUE(stats.max_depth_seen == 2, "Max depth");
    FATP_ASSERT_TRUE(stats.values_parsed >= 4, "Values parsed");

    return true;
}

// =============================================================================
// Convenience Function Tests
// =============================================================================

FATP_TEST_CASE(parse_cbor_convenience)
{
    auto data = encode_array_header(3);
    auto v1 = encode_uint(1);
    auto v2 = encode_uint(2);
    auto v3 = encode_uint(3);
    data.insert(data.end(), v1.begin(), v1.end());
    data.insert(data.end(), v2.begin(), v2.end());
    data.insert(data.end(), v3.begin(), v3.end());

    auto result = parse_cbor(data);
    FATP_ASSERT_TRUE(result.is_array(), "Is array");
    FATP_ASSERT_TRUE(result.as_array().size() == 3, "Size 3");

    return true;
}

// =============================================================================
// Benchmarks
// =============================================================================
} // namespace fat_p::testing::cborstreamlite

namespace fat_p::testing
{


inline void run_benchmarks()
{
    using namespace fat_p::testing;

    auto& out = *get_test_config().output;
    out << "\n" << colors::cyan() << "Sanity Benchmark:" << colors::reset() << "\n";
    out << "  (benchmarks are provided by benchmark executables; unit tests do not run full benchmarks)\n\n";
}

bool test_CborStreamLite()
{
    FATP_PRINT_HEADER(CBOR STREAM LITE)

    TestRunner runner;

    FATP_RUN_TEST_NS(runner, cborstreamlite, parse_unsigned_integers);
    FATP_RUN_TEST_NS(runner, cborstreamlite, parse_negative_integers);
    FATP_RUN_TEST_NS(runner, cborstreamlite, parse_byte_strings);
    FATP_RUN_TEST_NS(runner, cborstreamlite, parse_text_strings);
    FATP_RUN_TEST_NS(runner, cborstreamlite, parse_simple_values);
    FATP_RUN_TEST_NS(runner, cborstreamlite, parse_floats);
    FATP_RUN_TEST_NS(runner, cborstreamlite, parse_arrays);
    FATP_RUN_TEST_NS(runner, cborstreamlite, parse_maps);
    FATP_RUN_TEST_NS(runner, cborstreamlite, parse_nested);
    FATP_RUN_TEST_NS(runner, cborstreamlite, chunked_input);
    FATP_RUN_TEST_NS(runner, cborstreamlite, chunked_random_sizes);
    FATP_RUN_TEST_NS(runner, cborstreamlite, limit_max_depth);
    FATP_RUN_TEST_NS(runner, cborstreamlite, limit_string_size);
    FATP_RUN_TEST_NS(runner, cborstreamlite, limit_total_size);
    FATP_RUN_TEST_NS(runner, cborstreamlite, error_truncated_input);
    FATP_RUN_TEST_NS(runner, cborstreamlite, error_reserved_ai);
    FATP_RUN_TEST_NS(runner, cborstreamlite, error_indefinite_length);
    FATP_RUN_TEST_NS(runner, cborstreamlite, statistics);
    FATP_RUN_TEST_NS(runner, cborstreamlite, parse_cbor_convenience);


    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

// =============================================================================
// Standalone Entry Point
// =============================================================================
#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_CborStreamLite() ? 0 : 1;
}
#endif
