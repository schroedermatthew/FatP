/**
 * @file test_FatPJsonStream.cpp
 * @brief Comprehensive unit tests for FatPJsonStream.h
 */
/*
FATP_META:
  meta_version: 1
  component: FatPJsonStream
  file_role: test
  path: components/Json/tests/test_FatPJsonStream.cpp
  namespace: fat_p
  summary: "Unit tests for FatPJsonStream."
  api_stability: in_work
  related:
    docs_search: "FatPJsonStream"
    headers:
      - include/fat_p/FatPJsonStream.h
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
#include <cstring>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "FatPJsonStream.h"
#include "FatPTest.h"

namespace fat_p::testing::fatpjsonstream
{

using namespace fat_p::json_stream_fatp;

// =============================================================================
// Basic Parsing Tests
// =============================================================================

FATP_TEST_CASE(default_parser_basic)
{
    DefaultJsonStreamParser parser;

    auto result = parser.parse(R"({"key": 42})");

    FATP_ASSERT_TRUE(result.has_value(), "Parse succeeded");
    FATP_ASSERT_TRUE(result->is_object(), "Is object");
    FATP_ASSERT_TRUE(result->as_object().at("key").as_int() == 42, "Value 42");

    return true;
}

FATP_TEST_CASE(default_parser_array)
{
    DefaultJsonStreamParser parser;

    auto result = parser.parse("[1, 2, 3]");

    FATP_ASSERT_TRUE(result.has_value(), "Parse succeeded");
    FATP_ASSERT_TRUE(result->is_array(), "Is array");
    FATP_ASSERT_TRUE(result->as_array().size() == 3, "Size 3");

    return true;
}

FATP_TEST_CASE(strict_parser_basic)
{
    StrictJsonStreamParser parser;

    auto result = parser.parse(R"({"name": "test"})");

    FATP_ASSERT_TRUE(result.has_value(), "Parse succeeded");
    FATP_ASSERT_TRUE(result->as_object().at("name").as_string() == "test", "Value");

    return true;
}

FATP_TEST_CASE(relaxed_parser_basic)
{
    RelaxedJsonStreamParser parser;

    auto result = parser.parse("[1, 2, 3]");

    FATP_ASSERT_TRUE(result.has_value(), "Parse succeeded");
    FATP_ASSERT_TRUE(result->as_array().size() == 3, "Size 3");

    return true;
}

// =============================================================================
// Convenience Function Tests
// =============================================================================

FATP_TEST_CASE(stream_parse_json_convenience)
{
    auto result = stream_parse_json("[1, 2, 3]");

    FATP_ASSERT_TRUE(result.has_value(), "Parse succeeded");
    FATP_ASSERT_TRUE(result->as_array().size() == 3, "Size 3");

    return true;
}

FATP_TEST_CASE(stream_parse_json_strict_convenience)
{
    auto result = stream_parse_json_strict(R"({"key": "value"})");

    FATP_ASSERT_TRUE(result.has_value(), "Parse succeeded");
    FATP_ASSERT_TRUE(result->as_object().at("key").as_string() == "value", "Value");

    return true;
}

FATP_TEST_CASE(stream_parse_json_limited_convenience)
{
    RuntimeLimitsPolicy limits;
    limits.max_depth = 4;

    auto result = stream_parse_json_limited("[1]", limits);

    FATP_ASSERT_TRUE(result.has_value(), "Parse succeeded");

    return true;
}

// =============================================================================
// Policy Limit Tests
// =============================================================================

FATP_TEST_CASE(strict_limits_depth)
{
    StrictJsonStreamParser parser; // max_depth = 32

    // Create deeply nested JSON
    std::string json = "";
    for (int i = 0; i < 40; ++i)
    {
        json += "[";
    }
    json += "1";
    for (int i = 0; i < 40; ++i)
    {
        json += "]";
    }

    auto result = parser.parse(json);

    FATP_ASSERT_TRUE(!result.has_value(), "Parse failed");
    FATP_ASSERT_TRUE(result.error().code == ParseError::MaxDepthExceeded, "Depth err");

    return true;
}

FATP_TEST_CASE(strict_limits_string_size)
{
    StrictJsonStreamParser parser; // max_string_bytes = 64KB

    // Create string larger than 64KB
    std::string large = "[\"" + std::string(100 * 1024, 'x') + "\"]";

    auto result = parser.parse(large);

    FATP_ASSERT_TRUE(!result.has_value(), "Parse failed");
    FATP_ASSERT_TRUE(result.error().code == ParseError::MaxStringSizeExceeded, "Err");

    return true;
}

FATP_TEST_CASE(configurable_limits)
{
    RuntimeLimitsPolicy limits;
    limits.max_depth = 2;

    ConfigurableJsonStreamParser parser(limits);

    // Create 5-level deep nesting
    auto result = parser.parse("[[[[[1]]]]]");

    FATP_ASSERT_TRUE(!result.has_value(), "Parse failed");
    FATP_ASSERT_TRUE(result.error().code == ParseError::MaxDepthExceeded, "Depth err");

    return true;
}

// =============================================================================
// Chunked Feeding Tests
// =============================================================================

FATP_TEST_CASE(chunked_feeding)
{
    DefaultJsonStreamParser parser;

    std::string json = R"({"items": [1, 2, 3]})";

    // Feed byte by byte
    for (std::size_t i = 0; i < json.size() - 1; ++i)
    {
        auto status = parser.feed(json.data() + i, 1);
        FATP_ASSERT_TRUE(status.has_value(), "Feed succeeded");
        FATP_ASSERT_TRUE(*status == ParseStatus::NeedMoreData, "Need more");
    }

    // Last byte
    auto status = parser.feed(json.data() + json.size() - 1, 1);
    FATP_ASSERT_TRUE(status.has_value(), "Final feed succeeded");
    FATP_ASSERT_TRUE(*status == ParseStatus::Done, "Done");

    const auto& result = parser.result();
    FATP_ASSERT_TRUE(result.is_object(), "Is object");

    return true;
}

FATP_TEST_CASE(chunked_feeding_error)
{
    StrictJsonStreamParser parser;

    // Create deeply nested structure
    std::string json = "";
    for (int i = 0; i < 50; ++i)
    {
        json += "[";
    }

    bool got_error = false;
    for (std::size_t i = 0; i < json.size(); ++i)
    {
        auto status = parser.feed(json.data() + i, 1);
        if (!status.has_value())
        {
            got_error = true;
            FATP_ASSERT_TRUE(status.error().code == ParseError::MaxDepthExceeded, "Depth error");
            break;
        }
    }

    FATP_ASSERT_TRUE(got_error, "Error detected");

    return true;
}

// =============================================================================
// Progress Callback Tests
// =============================================================================

FATP_TEST_CASE(progress_callback)
{
    DefaultJsonStreamParser parser;

    int callback_count = 0;
    std::size_t last_bytes = 0;

    parser.set_progress_interval(10);
    parser.set_progress_callback([&](std::size_t bytes, std::size_t depth, std::size_t values) {
        ++callback_count;
        last_bytes = bytes;
        (void)depth;
        (void)values;
    });

    // Build larger JSON
    std::string json = "[";
    for (int i = 0; i < 50; ++i)
    {
        if (i > 0)
        {
            json += ",";
        }
        json += std::to_string(i);
    }
    json += "]";

    auto result = parser.parse(json);
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
    StrictJsonStreamParser parser;

    // Create deeply nested
    std::string json = "";
    for (int i = 0; i < 50; ++i)
    {
        json += "[";
    }

    auto result = parser.parse(json);
    FATP_ASSERT_TRUE(!result.has_value(), "Parse failed");

    const auto& error = result.error();
    FATP_ASSERT_TRUE(error.code == ParseError::MaxDepthExceeded, "Correct code");
    FATP_ASSERT_TRUE(error.byte_position > 0, "Position recorded");
    FATP_ASSERT_TRUE(!error.message.empty(), "Message present");

    std::string error_str = error.to_string();
    FATP_ASSERT_TRUE(error_str.find("line") != std::string::npos, "Has line");
    FATP_ASSERT_TRUE(error_str.find("column") != std::string::npos, "Has column");

    return true;
}

FATP_TEST_CASE(error_line_column)
{
    DefaultJsonStreamParser parser;

    // Error on line 3
    std::string json = "{\n  \"key\": \n  invalid\n}";

    auto result = parser.parse(json);
    FATP_ASSERT_TRUE(!result.has_value(), "Parse failed");
    FATP_ASSERT_TRUE(result.error().line == 3, "Line 3");

    return true;
}

FATP_TEST_CASE(incomplete_input_error)
{
    DefaultJsonStreamParser parser;

    auto result = parser.parse("[1, 2,"); // Incomplete

    FATP_ASSERT_TRUE(!result.has_value(), "Parse failed");
    FATP_ASSERT_TRUE(result.error().code == ParseError::UnexpectedEof, "EOF error");

    return true;
}

// =============================================================================
// Nested Structure Tests
// =============================================================================

FATP_TEST_CASE(nested_objects)
{
    DefaultJsonStreamParser parser;

    auto result = parser.parse(R"({"a": {"b": {"c": 123}}})");

    FATP_ASSERT_TRUE(result.has_value(), "Parse succeeded");

    int64_t val = result->as_object().at("a").as_object().at("b").as_object().at("c").as_int();
    FATP_ASSERT_TRUE(val == 123, "Deep value");

    return true;
}

FATP_TEST_CASE(mixed_nesting)
{
    DefaultJsonStreamParser parser;

    auto result = parser.parse(R"({"items": [{"id": 1}, {"id": 2}]})");

    FATP_ASSERT_TRUE(result.has_value(), "Parse succeeded");

    auto& items = result->as_object().at("items").as_array();
    FATP_ASSERT_TRUE(items.size() == 2, "Two items");
    FATP_ASSERT_TRUE(items[0].as_object().at("id").as_int() == 1, "First id");
    FATP_ASSERT_TRUE(items[1].as_object().at("id").as_int() == 2, "Second id");

    return true;
}

// =============================================================================

} // namespace fat_p::testing::fatpjsonstream

namespace fat_p::testing
{

bool test_FatPJsonStream()
{
    FATP_PRINT_HEADER(FATP JSON STREAM)

    TestRunner runner;

    FATP_RUN_TEST_NS(runner, fatpjsonstream, default_parser_basic);
    FATP_RUN_TEST_NS(runner, fatpjsonstream, default_parser_array);
    FATP_RUN_TEST_NS(runner, fatpjsonstream, strict_parser_basic);
    FATP_RUN_TEST_NS(runner, fatpjsonstream, relaxed_parser_basic);
    FATP_RUN_TEST_NS(runner, fatpjsonstream, stream_parse_json_convenience);
    FATP_RUN_TEST_NS(runner, fatpjsonstream, stream_parse_json_strict_convenience);
    FATP_RUN_TEST_NS(runner, fatpjsonstream, stream_parse_json_limited_convenience);
    FATP_RUN_TEST_NS(runner, fatpjsonstream, strict_limits_depth);
    FATP_RUN_TEST_NS(runner, fatpjsonstream, strict_limits_string_size);
    FATP_RUN_TEST_NS(runner, fatpjsonstream, configurable_limits);
    FATP_RUN_TEST_NS(runner, fatpjsonstream, chunked_feeding);
    FATP_RUN_TEST_NS(runner, fatpjsonstream, chunked_feeding_error);
    FATP_RUN_TEST_NS(runner, fatpjsonstream, progress_callback);
    FATP_RUN_TEST_NS(runner, fatpjsonstream, error_information);
    FATP_RUN_TEST_NS(runner, fatpjsonstream, error_line_column);
    FATP_RUN_TEST_NS(runner, fatpjsonstream, incomplete_input_error);
    FATP_RUN_TEST_NS(runner, fatpjsonstream, nested_objects);
    FATP_RUN_TEST_NS(runner, fatpjsonstream, mixed_nesting);


    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

// =============================================================================
// Standalone Entry Point
// =============================================================================
#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_FatPJsonStream() ? 0 : 1;
}
#endif
