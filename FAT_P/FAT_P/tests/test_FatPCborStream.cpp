/**
 * @file test_FatPCborStream.cpp
 * @brief Comprehensive unit tests for FatPCborStream.h
 */
/*
FATP_META:
  meta_version: 1
  component: FatPCborStream
  file_role: test
  path: tests/test_FatPCborStream.cpp
  namespace: fat_p
  summary: "Unit tests for FatPCborStream."
  related:
    docs_search: "FatPCborStream"
    headers:
      - fat_p/FatPCborStream.h
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
#include <cstring>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "FatPCborStream.h"
#include "FatPTest.h"

namespace fat_p::testing::fatpcborstream
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

// =============================================================================
// Basic Parsing Tests
// =============================================================================

FATP_TEST_CASE(default_parser_basic)
{
    DefaultStreamParser parser;

    auto data = encode_uint(42);
    auto result = parser.parse(data);

    FATP_ASSERT_TRUE(result.has_value(), "Parse succeeded");
    FATP_ASSERT_TRUE(result->is_unsigned(), "Is unsigned");
    FATP_ASSERT_TRUE(result->as_unsigned() == 42, "Value 42");

    return true;
}

FATP_TEST_CASE(default_parser_array)
{
    DefaultStreamParser parser;

    auto data = encode_array_header(3);
    auto v1 = encode_uint(1);
    auto v2 = encode_uint(2);
    auto v3 = encode_uint(3);
    data.insert(data.end(), v1.begin(), v1.end());
    data.insert(data.end(), v2.begin(), v2.end());
    data.insert(data.end(), v3.begin(), v3.end());

    auto result = parser.parse(data);

    FATP_ASSERT_TRUE(result.has_value(), "Parse succeeded");
    FATP_ASSERT_TRUE(result->is_array(), "Is array");
    FATP_ASSERT_TRUE(result->as_array().size() == 3, "Array size");

    return true;
}

FATP_TEST_CASE(strict_parser_basic)
{
    StrictStreamParser parser;

    auto data = encode_text("hello");
    auto result = parser.parse(data);

    FATP_ASSERT_TRUE(result.has_value(), "Parse succeeded");
    FATP_ASSERT_TRUE(result->is_string(), "Is string");
    FATP_ASSERT_TRUE(result->as_string() == "hello", "Value hello");

    return true;
}

FATP_TEST_CASE(relaxed_parser_basic)
{
    RelaxedStreamParser parser;

    auto data = encode_uint(12345);
    auto result = parser.parse(data);

    FATP_ASSERT_TRUE(result.has_value(), "Parse succeeded");
    FATP_ASSERT_TRUE(result->as_unsigned() == 12345, "Value 12345");

    return true;
}

// =============================================================================
// Convenience Function Tests
// =============================================================================

FATP_TEST_CASE(stream_parse_convenience)
{
    auto data = encode_uint(100);
    auto result = stream_parse(data);

    FATP_ASSERT_TRUE(result.has_value(), "Parse succeeded");
    FATP_ASSERT_TRUE(result->as_unsigned() == 100, "Value 100");

    return true;
}

FATP_TEST_CASE(stream_parse_strict_convenience)
{
    auto data = encode_text("test");
    auto result = stream_parse_strict(data);

    FATP_ASSERT_TRUE(result.has_value(), "Parse succeeded");
    FATP_ASSERT_TRUE(result->as_string() == "test", "Value test");

    return true;
}

FATP_TEST_CASE(stream_parse_limited_convenience)
{
    RuntimeLimitsPolicy limits;
    limits.max_depth = 4;

    auto data = encode_uint(999);
    auto result = stream_parse_limited(data, limits);

    FATP_ASSERT_TRUE(result.has_value(), "Parse succeeded");
    FATP_ASSERT_TRUE(result->as_unsigned() == 999, "Value 999");

    return true;
}

// =============================================================================
// Policy Limit Tests
// =============================================================================

FATP_TEST_CASE(strict_limits_depth)
{
    StrictStreamParser parser; // max_depth = 32

    // Create array nested 40 deep (exceeds strict limit of 32)
    std::vector<uint8_t> data;
    for (int i = 0; i < 40; ++i)
    {
        auto hdr = encode_array_header(1);
        data.insert(data.end(), hdr.begin(), hdr.end());
    }
    auto val = encode_uint(1);
    data.insert(data.end(), val.begin(), val.end());

    auto result = parser.parse(data);

    FATP_ASSERT_TRUE(!result.has_value(), "Parse failed");
    FATP_ASSERT_TRUE(result.error().code == ParseError::MaxDepthExceeded, "Depth error");

    return true;
}

FATP_TEST_CASE(strict_limits_string_size)
{
    StrictStreamParser parser; // max_string_bytes = 64KB

    // Create string larger than 64KB
    std::string large_string(100 * 1024, 'x');
    auto data = encode_text(large_string);

    auto result = parser.parse(data);

    FATP_ASSERT_TRUE(!result.has_value(), "Parse failed");
    FATP_ASSERT_TRUE(result.error().code == ParseError::MaxStringSizeExceeded, "String size error");

    return true;
}

FATP_TEST_CASE(configurable_limits)
{
    RuntimeLimitsPolicy limits;
    limits.max_depth = 2;

    ConfigurableStreamParser parser(limits);

    // Create array nested 5 deep (exceeds limit of 2)
    std::vector<uint8_t> data;
    for (int i = 0; i < 5; ++i)
    {
        auto hdr = encode_array_header(1);
        data.insert(data.end(), hdr.begin(), hdr.end());
    }
    auto val = encode_uint(1);
    data.insert(data.end(), val.begin(), val.end());

    auto result = parser.parse(data);

    FATP_ASSERT_TRUE(!result.has_value(), "Parse failed");
    FATP_ASSERT_TRUE(result.error().code == ParseError::MaxDepthExceeded, "Depth error");

    return true;
}

// =============================================================================
// UTF-8 Validation Tests
// =============================================================================

FATP_TEST_CASE(utf8_validation_valid)
{
    ValidatingStreamParser parser;

    // Valid UTF-8: ASCII
    auto data = encode_text("Hello, World!");
    auto result = parser.parse(data);
    FATP_ASSERT_TRUE(result.has_value(), "ASCII valid");

    // Valid UTF-8: Multi-byte
    parser.reset();
    data = encode_text("Hello, \xC3\xA9\xC3\xA0\xC3\xBC"); // e-acute, a-grave, u-umlaut
    result = parser.parse(data);
    FATP_ASSERT_TRUE(result.has_value(), "Multi-byte valid");

    return true;
}

FATP_TEST_CASE(utf8_validation_invalid)
{
    ValidatingStreamParser parser;

    // Invalid UTF-8: Lone continuation byte
    auto data = encode_text("Hello\x80World");
    auto result = parser.parse(data);
    FATP_ASSERT_TRUE(!result.has_value(), "Invalid UTF-8 rejected");
    FATP_ASSERT_TRUE(result.error().code == ParseError::InvalidUtf8, "UTF-8 error");

    return true;
}

FATP_TEST_CASE(utf8_validation_overlong)
{
    ValidatingStreamParser parser;

    // Overlong encoding of '/' (should be 0x2F, not C0 AF)
    auto data = encode_text("test\xC0\xAF");
    auto result = parser.parse(data);
    FATP_ASSERT_TRUE(!result.has_value(), "Overlong rejected");

    return true;
}

// =============================================================================
// Chunked Feeding Tests
// =============================================================================

FATP_TEST_CASE(chunked_feeding)
{
    DefaultStreamParser parser;

    // Build test data
    auto data = encode_array_header(2);
    auto v1 = encode_text("hello");
    auto v2 = encode_uint(42);
    data.insert(data.end(), v1.begin(), v1.end());
    data.insert(data.end(), v2.begin(), v2.end());

    // Feed byte by byte
    for (size_t i = 0; i < data.size() - 1; ++i)
    {
        auto status = parser.feed(&data[i], 1);
        FATP_ASSERT_TRUE(status.has_value(), "Feed succeeded");
        FATP_ASSERT_TRUE(*status == ParseStatus::NeedMoreData, "Need more data");
    }

    // Last byte
    auto status = parser.feed(&data.back(), 1);
    FATP_ASSERT_TRUE(status.has_value(), "Final feed succeeded");
    FATP_ASSERT_TRUE(*status == ParseStatus::Done, "Parsing done");

    // Verify result
    const auto& result = parser.result();
    FATP_ASSERT_TRUE(result.is_array(), "Is array");
    FATP_ASSERT_TRUE(result.as_array().size() == 2, "Array size 2");

    return true;
}

FATP_TEST_CASE(chunked_feeding_error)
{
    StrictStreamParser parser;

    // Start feeding a deeply nested structure
    std::vector<uint8_t> data;
    for (int i = 0; i < 50; ++i)
    {
        auto hdr = encode_array_header(1);
        data.insert(data.end(), hdr.begin(), hdr.end());
    }

    // Feed until error
    bool got_error = false;
    for (size_t i = 0; i < data.size(); ++i)
    {
        auto status = parser.feed(&data[i], 1);
        if (!status.has_value())
        {
            got_error = true;
            FATP_ASSERT_TRUE(status.error().code == ParseError::MaxDepthExceeded, "Depth error");
            break;
        }
    }

    FATP_ASSERT_TRUE(got_error, "Error detected during chunked feed");

    return true;
}

// =============================================================================
// Progress Callback Tests
// =============================================================================

FATP_TEST_CASE(progress_callback)
{
    DefaultStreamParser parser;

    int callback_count = 0;
    std::size_t last_bytes = 0;

    parser.set_progress_interval(10);
    parser.set_progress_callback(
        [&](std::size_t bytes, std::size_t depth, std::size_t values)
        {
            ++callback_count;
            last_bytes = bytes;
            (void)depth;
            (void)values;
        });

    // Build data larger than progress interval
    std::vector<uint8_t> data = encode_array_header(50);
    for (int i = 0; i < 50; ++i)
    {
        auto v = encode_uint(static_cast<uint64_t>(i));
        data.insert(data.end(), v.begin(), v.end());
    }

    auto result = parser.parse(data);
    FATP_ASSERT_TRUE(result.has_value(), "Parse succeeded");
    FATP_ASSERT_TRUE(callback_count > 0, "Callbacks fired");
    FATP_ASSERT_TRUE(last_bytes > 0, "Bytes reported");

    return true;
}

// =============================================================================
// Error Information Tests
// =============================================================================

FATP_TEST_CASE(error_information)
{
    StrictStreamParser parser;

    // Create data that exceeds depth limit
    std::vector<uint8_t> data;
    for (int i = 0; i < 50; ++i)
    {
        auto hdr = encode_array_header(1);
        data.insert(data.end(), hdr.begin(), hdr.end());
    }

    auto result = parser.parse(data);
    FATP_ASSERT_TRUE(!result.has_value(), "Parse failed");

    const auto& error = result.error();
    FATP_ASSERT_TRUE(error.code == ParseError::MaxDepthExceeded, "Correct error code");
    FATP_ASSERT_TRUE(error.byte_position > 0, "Position recorded");
    FATP_ASSERT_TRUE(!error.message.empty(), "Message present");

    std::string error_str = error.to_string();
    FATP_ASSERT_TRUE(error_str.find("depth") != std::string::npos, "Error string has context");

    return true;
}

FATP_TEST_CASE(incomplete_input_error)
{
    DefaultStreamParser parser;

    // Start of an array but no elements
    auto data = encode_array_header(3);
    // Don't add the 3 elements

    auto result = parser.parse(data);
    FATP_ASSERT_TRUE(!result.has_value(), "Parse failed");
    FATP_ASSERT_TRUE(result.error().code == ParseError::UnexpectedEof, "EOF error");

    return true;
}

// =============================================================================
// USING Macro Test
// =============================================================================

FATP_TEST_CASE(using_macro)
{
    USING_FATP_CBOR_STREAM();

    auto data = encode_uint(123);
    auto result = stream_parse(data);

    FATP_ASSERT_TRUE(result.has_value(), "Parse with macro succeeded");
    FATP_ASSERT_TRUE(result->as_unsigned() == 123, "Value 123");

    return true;
}

// =============================================================================
// Benchmarks
// =============================================================================

void benchmark_policies()
{
    std::cout << "\n" << colors::cyan() << "FatPCborStream Policy Benchmarks:" << colors::reset() << "\n\n";

    constexpr int iterations = 1000;
    constexpr int warmup = 100;

    // Build test data
    std::vector<uint8_t> data = encode_array_header(100);
    for (int i = 0; i < 100; ++i)
    {
        auto inner = encode_map_header(3);
        auto k1 = encode_text("id");
        auto v1 = encode_uint(static_cast<uint64_t>(i));
        auto k2 = encode_text("name");
        auto v2 = encode_text("item_" + std::to_string(i));
        auto k3 = encode_text("value");
        auto v3 = encode_uint(static_cast<uint64_t>(i * 100));
        inner.insert(inner.end(), k1.begin(), k1.end());
        inner.insert(inner.end(), v1.begin(), v1.end());
        inner.insert(inner.end(), k2.begin(), k2.end());
        inner.insert(inner.end(), v2.begin(), v2.end());
        inner.insert(inner.end(), k3.begin(), k3.end());
        inner.insert(inner.end(), v3.begin(), v3.end());
        data.insert(data.end(), inner.begin(), inner.end());
    }

    std::cout << "Test data size: " << data.size() << " bytes\n\n";

    // Default parser (no validation)
    DefaultStreamParser default_parser;
    double time_default = measure_perf(
        [&]()
        {
            default_parser.reset();
            auto result = default_parser.parse(data);
            DoNotOptimize(result);
        },
        iterations,
        warmup);

    std::cout << "DefaultStreamParser:    " << format_time(time_default) << "\n";

    // Validating parser (UTF-8)
    ValidatingStreamParser validating_parser;
    double time_validating = measure_perf(
        [&]()
        {
            validating_parser.reset();
            auto result = validating_parser.parse(data);
            DoNotOptimize(result);
        },
        iterations,
        warmup);

    std::cout << "ValidatingStreamParser: " << format_time(time_validating) << "\n";

    // Strict parser (UTF-8 + ordering)
    StrictStreamParser strict_parser;
    double time_strict = measure_perf(
        [&]()
        {
            strict_parser.reset();
            auto result = strict_parser.parse(data);
            DoNotOptimize(result);
        },
        iterations,
        warmup);

    std::cout << "StrictStreamParser:     " << format_time(time_strict) << "\n";

    // Calculate throughput
    double throughput_default = data.size() / time_default / 1000.0;
    double throughput_validating = data.size() / time_validating / 1000.0;
    double throughput_strict = data.size() / time_strict / 1000.0;

    std::cout << "\nThroughput (default):    " << std::fixed << std::setprecision(1) << throughput_default << " MB/s\n";
    std::cout << "Throughput (validating): " << std::fixed << std::setprecision(1) << throughput_validating
              << " MB/s\n";
    std::cout << "Throughput (strict):     " << std::fixed << std::setprecision(1) << throughput_strict << " MB/s\n";

    // Overhead calculation
    double validation_overhead = (time_validating - time_default) / time_default * 100.0;
    double strict_overhead = (time_strict - time_default) / time_default * 100.0;

    std::cout << "\nValidation overhead:     " << std::fixed << std::setprecision(1) << validation_overhead << "%\n";
    std::cout << "Strict overhead:         " << std::fixed << std::setprecision(1) << strict_overhead << "%\n";
}

} // namespace fat_p::testing::fatpcborstream

namespace fat_p::testing
{

bool test_FatPCborStream()
{
    FATP_PRINT_HEADER(FATP CBOR STREAM)

    TestRunner runner;

    FATP_RUN_TEST_NS(runner, fatpcborstream, default_parser_basic);
    FATP_RUN_TEST_NS(runner, fatpcborstream, default_parser_array);
    FATP_RUN_TEST_NS(runner, fatpcborstream, strict_parser_basic);
    FATP_RUN_TEST_NS(runner, fatpcborstream, relaxed_parser_basic);
    FATP_RUN_TEST_NS(runner, fatpcborstream, stream_parse_convenience);
    FATP_RUN_TEST_NS(runner, fatpcborstream, stream_parse_strict_convenience);
    FATP_RUN_TEST_NS(runner, fatpcborstream, stream_parse_limited_convenience);
    FATP_RUN_TEST_NS(runner, fatpcborstream, strict_limits_depth);
    FATP_RUN_TEST_NS(runner, fatpcborstream, strict_limits_string_size);
    FATP_RUN_TEST_NS(runner, fatpcborstream, configurable_limits);
    FATP_RUN_TEST_NS(runner, fatpcborstream, utf8_validation_valid);
    FATP_RUN_TEST_NS(runner, fatpcborstream, utf8_validation_invalid);
    FATP_RUN_TEST_NS(runner, fatpcborstream, utf8_validation_overlong);
    FATP_RUN_TEST_NS(runner, fatpcborstream, chunked_feeding);
    FATP_RUN_TEST_NS(runner, fatpcborstream, chunked_feeding_error);
    FATP_RUN_TEST_NS(runner, fatpcborstream, progress_callback);
    FATP_RUN_TEST_NS(runner, fatpcborstream, error_information);
    FATP_RUN_TEST_NS(runner, fatpcborstream, incomplete_input_error);
    FATP_RUN_TEST_NS(runner, fatpcborstream, using_macro);

    fatpcborstream::benchmark_policies();

    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

// =============================================================================
// Standalone Entry Point
// =============================================================================
#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_FatPCborStream() ? 0 : 1;
}
#endif
