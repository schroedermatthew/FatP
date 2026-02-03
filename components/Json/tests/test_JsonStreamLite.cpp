/**
 * @file test_JsonStreamLite.cpp
 * @brief Comprehensive unit tests for JsonStreamLite.h
 */
/*
FATP_META:
  meta_version: 1
  component: JsonStreamLite
  file_role: test
  path: components/Json/tests/test_JsonStreamLite.cpp
  layer: Testing
  namespace: fat_p
  summary: "Unit tests for JsonStreamLite."
  api_stability: in_work
  related:
    docs_search: "JsonStreamLite"
    headers:
      - include/fat_p/JsonStreamLite.h
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

#include "FatPTest.h"
#include "JsonStreamLite.h"

namespace fat_p::testing::jsonstreamlite
{

using namespace fat_p::json_stream;

// =============================================================================
// Basic Type Tests
// =============================================================================

FATP_TEST_CASE(parse_integers)
{
    JsonStreamParser parser;
    const char* json = "[0, 1, -1, 42, -42, 2147483647, -2147483648]";
    auto status = parser.feed(json, strlen(json));

    FATP_ASSERT_TRUE(status == ParseStatus::Done, "Parse succeeded");

    auto result = parser.take_result();
    FATP_ASSERT_TRUE(result.is_array(), "Is array");

    auto& arr = result.as_array();
    FATP_ASSERT_TRUE(arr[0].as_int() == 0, "Zero");
    FATP_ASSERT_TRUE(arr[1].as_int() == 1, "One");
    FATP_ASSERT_TRUE(arr[2].as_int() == -1, "Negative one");
    FATP_ASSERT_TRUE(arr[3].as_int() == 42, "42");
    FATP_ASSERT_TRUE(arr[4].as_int() == -42, "-42");
    FATP_ASSERT_TRUE(arr[5].as_int() == 2147483647, "INT_MAX");
    FATP_ASSERT_TRUE(arr[6].as_int() == -2147483648LL, "INT_MIN");

    return true;
}

FATP_TEST_CASE(parse_doubles)
{
    JsonStreamParser parser;
    const char* json = "[3.14, -2.5, 1.0e10, 1.5e-5, 0.0]";
    auto status = parser.feed(json, strlen(json));

    FATP_ASSERT_TRUE(status == ParseStatus::Done, "Parse succeeded");

    auto result = parser.take_result();
    auto& arr = result.as_array();

    FATP_ASSERT_TRUE(std::abs(arr[0].as_double() - 3.14) < 1e-10, "3.14");
    FATP_ASSERT_TRUE(std::abs(arr[1].as_double() - (-2.5)) < 1e-10, "-2.5");
    FATP_ASSERT_TRUE(std::abs(arr[2].as_double() - 1.0e10) < 1e5, "1e10");
    FATP_ASSERT_TRUE(std::abs(arr[3].as_double() - 1.5e-5) < 1e-10, "1.5e-5");
    FATP_ASSERT_TRUE(arr[4].as_double() == 0.0, "0.0");

    return true;
}

FATP_TEST_CASE(parse_strings)
{
    JsonStreamParser parser;
    const char* json = R"(["hello", "", "with spaces", "unicode: \u0041"])";
    auto status = parser.feed(json, strlen(json));

    FATP_ASSERT_TRUE(status == ParseStatus::Done, "Parse succeeded");

    auto result = parser.take_result();
    auto& arr = result.as_array();

    FATP_ASSERT_TRUE(arr[0].as_string() == "hello", "hello");
    FATP_ASSERT_TRUE(arr[1].as_string() == "", "empty");
    FATP_ASSERT_TRUE(arr[2].as_string() == "with spaces", "spaces");
    FATP_ASSERT_TRUE(arr[3].as_string() == "unicode: A", "unicode");

    return true;
}

FATP_TEST_CASE(parse_escape_sequences)
{
    JsonStreamParser parser;
    const char* json = R"(["a\nb", "a\tb", "a\"b", "a\\b", "a\/b", "a\rb", "a\fb", "a\bb"])";
    auto status = parser.feed(json, strlen(json));

    FATP_ASSERT_TRUE(status == ParseStatus::Done, "Parse succeeded");

    auto result = parser.take_result();
    auto& arr = result.as_array();

    FATP_ASSERT_TRUE(arr[0].as_string() == "a\nb", "newline");
    FATP_ASSERT_TRUE(arr[1].as_string() == "a\tb", "tab");
    FATP_ASSERT_TRUE(arr[2].as_string() == "a\"b", "quote");
    FATP_ASSERT_TRUE(arr[3].as_string() == "a\\b", "backslash");
    FATP_ASSERT_TRUE(arr[4].as_string() == "a/b", "slash");
    FATP_ASSERT_TRUE(arr[5].as_string() == "a\rb", "carriage return");
    FATP_ASSERT_TRUE(arr[6].as_string() == "a\fb", "form feed");
    FATP_ASSERT_TRUE(arr[7].as_string() == "a\bb", "backspace");

    return true;
}

FATP_TEST_CASE(parse_literals)
{
    JsonStreamParser parser;
    const char* json = "[true, false, null]";
    auto status = parser.feed(json, strlen(json));

    FATP_ASSERT_TRUE(status == ParseStatus::Done, "Parse succeeded");

    auto result = parser.take_result();
    auto& arr = result.as_array();

    FATP_ASSERT_TRUE(arr[0].is_bool() && arr[0].as_bool() == true, "true");
    FATP_ASSERT_TRUE(arr[1].is_bool() && arr[1].as_bool() == false, "false");
    FATP_ASSERT_TRUE(arr[2].is_null(), "null");

    return true;
}

// =============================================================================
// Container Tests
// =============================================================================

FATP_TEST_CASE(parse_empty_containers)
{
    JsonStreamParser parser1;
    auto s1 = parser1.feed("[]", 2);
    FATP_ASSERT_TRUE(s1 == ParseStatus::Done, "Empty array");
    FATP_ASSERT_TRUE(parser1.result().is_array(), "Is array");
    FATP_ASSERT_TRUE(parser1.result().as_array().empty(), "Array empty");

    JsonStreamParser parser2;
    auto s2 = parser2.feed("{}", 2);
    FATP_ASSERT_TRUE(s2 == ParseStatus::Done, "Empty object");
    FATP_ASSERT_TRUE(parser2.result().is_object(), "Is object");
    FATP_ASSERT_TRUE(parser2.result().as_object().empty(), "Object empty");

    return true;
}

FATP_TEST_CASE(parse_simple_array)
{
    JsonStreamParser parser;
    const char* json = "[1, 2, 3]";
    auto status = parser.feed(json, strlen(json));

    FATP_ASSERT_TRUE(status == ParseStatus::Done, "Parse succeeded");

    auto result = parser.take_result();
    FATP_ASSERT_TRUE(result.is_array(), "Is array");

    auto& arr = result.as_array();
    FATP_ASSERT_TRUE(arr.size() == 3, "Size 3");
    FATP_ASSERT_TRUE(arr[0].as_int() == 1, "First");
    FATP_ASSERT_TRUE(arr[1].as_int() == 2, "Second");
    FATP_ASSERT_TRUE(arr[2].as_int() == 3, "Third");

    return true;
}

FATP_TEST_CASE(parse_simple_object)
{
    JsonStreamParser parser;
    const char* json = R"({"name": "test", "value": 42})";
    auto status = parser.feed(json, strlen(json));

    FATP_ASSERT_TRUE(status == ParseStatus::Done, "Parse succeeded");

    auto result = parser.take_result();
    FATP_ASSERT_TRUE(result.is_object(), "Is object");

    auto& obj = result.as_object();
    FATP_ASSERT_TRUE(obj.at("name").as_string() == "test", "name");
    FATP_ASSERT_TRUE(obj.at("value").as_int() == 42, "value");

    return true;
}

FATP_TEST_CASE(parse_nested_arrays)
{
    JsonStreamParser parser;
    const char* json = "[[1, 2], [3, 4], [5, 6]]";
    auto status = parser.feed(json, strlen(json));

    FATP_ASSERT_TRUE(status == ParseStatus::Done, "Parse succeeded");

    auto result = parser.take_result();
    FATP_ASSERT_TRUE(result.as_array().size() == 3, "Outer size");
    FATP_ASSERT_TRUE(result.as_array()[0].as_array()[0].as_int() == 1, "First");
    FATP_ASSERT_TRUE(result.as_array()[2].as_array()[1].as_int() == 6, "Last");

    return true;
}

FATP_TEST_CASE(parse_nested_objects)
{
    JsonStreamParser parser;
    const char* json = R"({"outer": {"inner": {"deep": 99}}})";
    auto status = parser.feed(json, strlen(json));

    FATP_ASSERT_TRUE(status == ParseStatus::Done, "Parse succeeded");

    auto result = parser.take_result();
    auto val = result.as_object().at("outer").as_object().at("inner").as_object().at("deep").as_int();
    FATP_ASSERT_TRUE(val == 99, "Deep value");

    return true;
}

FATP_TEST_CASE(parse_mixed_nesting)
{
    JsonStreamParser parser;
    const char* json = R"({"items": [{"id": 1}, {"id": 2}], "count": 2})";
    auto status = parser.feed(json, strlen(json));

    FATP_ASSERT_TRUE(status == ParseStatus::Done, "Parse succeeded");

    auto result = parser.take_result();
    auto& items = result.as_object().at("items").as_array();

    FATP_ASSERT_TRUE(items.size() == 2, "Items size");
    FATP_ASSERT_TRUE(items[0].as_object().at("id").as_int() == 1, "First id");
    FATP_ASSERT_TRUE(items[1].as_object().at("id").as_int() == 2, "Second id");
    FATP_ASSERT_TRUE(result.as_object().at("count").as_int() == 2, "Count");

    return true;
}

// =============================================================================
// Streaming Tests
// =============================================================================

FATP_TEST_CASE(chunked_input_byte_by_byte)
{
    JsonStreamParser parser;
    const char* json = R"({"key": [1, 2, 3]})";
    std::size_t len = strlen(json);

    ParseStatus status = ParseStatus::NeedMoreData;
    for (std::size_t i = 0; i < len; ++i)
    {
        status = parser.feed(json + i, 1);
        FATP_ASSERT_TRUE(status != ParseStatus::Error, "No error at byte");
    }

    FATP_ASSERT_TRUE(status == ParseStatus::Done, "Done");
    FATP_ASSERT_TRUE(parser.result().as_object().at("key").as_array().size() == 3, "Array");

    return true;
}

FATP_TEST_CASE(chunked_input_random_sizes)
{
    JsonStreamParser parser;
    const char* json = R"({"array": [1, 2, 3], "string": "hello", "number": 42})";
    std::size_t len = strlen(json);

    std::size_t chunk_sizes[] = {3, 7, 2, 11, 5, 1, 100};
    std::size_t pos = 0;
    std::size_t chunk_idx = 0;
    ParseStatus status = ParseStatus::NeedMoreData;

    while (pos < len && status == ParseStatus::NeedMoreData)
    {
        std::size_t chunk = chunk_sizes[chunk_idx % 7];
        if (pos + chunk > len)
        {
            chunk = len - pos;
        }

        status = parser.feed(json + pos, chunk);
        FATP_ASSERT_TRUE(status != ParseStatus::Error, "No error");

        pos += chunk;
        ++chunk_idx;
    }

    FATP_ASSERT_TRUE(status == ParseStatus::Done, "Done");

    return true;
}

// =============================================================================
// Limit Tests
// =============================================================================

FATP_TEST_CASE(limit_max_depth)
{
    JsonStreamParser parser;
    parser.set_max_depth(3);

    // Create 5-level deep array
    const char* json = "[[[[[1]]]]]";
    auto status = parser.feed(json, strlen(json));

    FATP_ASSERT_TRUE(status == ParseStatus::Error, "Error");
    FATP_ASSERT_TRUE(parser.error() == ParseError::MaxDepthExceeded, "Depth error");

    return true;
}

FATP_TEST_CASE(limit_string_size)
{
    JsonStreamParser parser;
    parser.set_max_string_size(10);

    std::string json = "[\"" + std::string(20, 'x') + "\"]";
    auto status = parser.feed(json.data(), json.size());

    FATP_ASSERT_TRUE(status == ParseStatus::Error, "Error");
    FATP_ASSERT_TRUE(parser.error() == ParseError::MaxStringSizeExceeded, "String error");

    return true;
}

FATP_TEST_CASE(limit_total_size)
{
    JsonStreamParser parser;
    parser.set_max_total_size(20);

    const char* json = R"({"key": "this is a very long value"})";
    auto status = parser.feed(json, strlen(json));

    FATP_ASSERT_TRUE(status == ParseStatus::Error, "Error");
    FATP_ASSERT_TRUE(parser.error() == ParseError::MaxTotalSizeExceeded, "Total error");

    return true;
}

// =============================================================================
// Error Tests
// =============================================================================

FATP_TEST_CASE(error_invalid_json)
{
    JsonStreamParser parser1;
    auto s1 = parser1.feed("invalid", 7);
    FATP_ASSERT_TRUE(s1 == ParseStatus::Error, "Invalid literal");

    JsonStreamParser parser2;
    auto s2 = parser2.feed("[1,]", 4);
    FATP_ASSERT_TRUE(s2 == ParseStatus::Error, "Trailing comma in array");

    JsonStreamParser parser3;
    auto s3 = parser3.feed("{\"key\":}", 8);
    FATP_ASSERT_TRUE(s3 == ParseStatus::Error, "Missing value");

    return true;
}

FATP_TEST_CASE(error_invalid_number)
{
    JsonStreamParser parser1;
    auto s1 = parser1.feed("[01]", 4);
    FATP_ASSERT_TRUE(s1 == ParseStatus::Error, "Leading zero");
    FATP_ASSERT_TRUE(parser1.error() == ParseError::InvalidNumber, "Number error");

    return true;
}

FATP_TEST_CASE(error_invalid_escape)
{
    JsonStreamParser parser;
    auto status = parser.feed(R"json(["test\q"])json", 10);
    FATP_ASSERT_TRUE(status == ParseStatus::Error, "Invalid escape");
    FATP_ASSERT_TRUE(parser.error() == ParseError::InvalidEscapeSequence, "Escape error");

    return true;
}

FATP_TEST_CASE(error_truncated_input)
{
    JsonStreamParser parser;
    auto status = parser.feed("[1, 2", 5);
    FATP_ASSERT_TRUE(status == ParseStatus::NeedMoreData, "Need more data");
    FATP_ASSERT_TRUE(!parser.is_done(), "Not done");

    return true;
}

// =============================================================================
// Statistics Tests
// =============================================================================

FATP_TEST_CASE(statistics_tracking)
{
    JsonStreamParser parser;
    const char* json = R"({"a": [1, 2], "b": {"c": 3}})";
    auto status = parser.feed(json, strlen(json));

    FATP_ASSERT_TRUE(status == ParseStatus::Done, "Done");

    const auto& stats = parser.stats();
    FATP_ASSERT_TRUE(stats.bytes_consumed == strlen(json), "Bytes");
    FATP_ASSERT_TRUE(stats.max_depth_seen >= 2, "Max depth");
    FATP_ASSERT_TRUE(stats.values_parsed > 0, "Values parsed");

    return true;
}

// =============================================================================
// Convenience Function Tests
// =============================================================================

FATP_TEST_CASE(parse_json_convenience)
{
    auto result = parse_json("[1, 2, 3]");
    FATP_ASSERT_TRUE(result.is_array(), "Is array");
    FATP_ASSERT_TRUE(result.as_array().size() == 3, "Size");

    std::string json_str = R"({"key": "value"})";
    auto result2 = parse_json(json_str);
    FATP_ASSERT_TRUE(result2.as_object().at("key").as_string() == "value", "Value");

    return true;
}

FATP_TEST_CASE(using_macro)
{
    FATP_USING_JSON_STREAM_LITE();

    auto result = parse_json("[true, false]");
    FATP_ASSERT_TRUE(result.is_array(), "Is array");
    FATP_ASSERT_TRUE(result.as_array()[0].as_bool() == true, "True");

    return true;
}

// =============================================================================
// Whitespace Tests
// =============================================================================

FATP_TEST_CASE(parse_with_whitespace)
{
    JsonStreamParser parser;
    const char* json = R"(
        {
            "key" : "value" ,
            "array" : [ 1 , 2 , 3 ]
        }
    )";
    auto status = parser.feed(json, strlen(json));

    FATP_ASSERT_TRUE(status == ParseStatus::Done, "Done");
    FATP_ASSERT_TRUE(parser.result().as_object().at("key").as_string() == "value", "Key");

    return true;
}

} // namespace fat_p::testing::jsonstreamlite

// =============================================================================

namespace fat_p::testing
{

bool test_JsonStreamLite()
{
    FATP_PRINT_HEADER(JSON STREAM LITE)

    TestRunner runner;

    FATP_RUN_TEST_NS(runner, jsonstreamlite, parse_integers);
    FATP_RUN_TEST_NS(runner, jsonstreamlite, parse_doubles);
    FATP_RUN_TEST_NS(runner, jsonstreamlite, parse_strings);
    FATP_RUN_TEST_NS(runner, jsonstreamlite, parse_escape_sequences);
    FATP_RUN_TEST_NS(runner, jsonstreamlite, parse_literals);
    FATP_RUN_TEST_NS(runner, jsonstreamlite, parse_empty_containers);
    FATP_RUN_TEST_NS(runner, jsonstreamlite, parse_simple_array);
    FATP_RUN_TEST_NS(runner, jsonstreamlite, parse_simple_object);
    FATP_RUN_TEST_NS(runner, jsonstreamlite, parse_nested_arrays);
    FATP_RUN_TEST_NS(runner, jsonstreamlite, parse_nested_objects);
    FATP_RUN_TEST_NS(runner, jsonstreamlite, parse_mixed_nesting);
    FATP_RUN_TEST_NS(runner, jsonstreamlite, chunked_input_byte_by_byte);
    FATP_RUN_TEST_NS(runner, jsonstreamlite, chunked_input_random_sizes);
    FATP_RUN_TEST_NS(runner, jsonstreamlite, limit_max_depth);
    FATP_RUN_TEST_NS(runner, jsonstreamlite, limit_string_size);
    FATP_RUN_TEST_NS(runner, jsonstreamlite, limit_total_size);
    FATP_RUN_TEST_NS(runner, jsonstreamlite, error_invalid_json);
    FATP_RUN_TEST_NS(runner, jsonstreamlite, error_invalid_number);
    FATP_RUN_TEST_NS(runner, jsonstreamlite, error_invalid_escape);
    FATP_RUN_TEST_NS(runner, jsonstreamlite, error_truncated_input);
    FATP_RUN_TEST_NS(runner, jsonstreamlite, statistics_tracking);
    FATP_RUN_TEST_NS(runner, jsonstreamlite, parse_json_convenience);
    FATP_RUN_TEST_NS(runner, jsonstreamlite, using_macro);
    FATP_RUN_TEST_NS(runner, jsonstreamlite, parse_with_whitespace);


    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

// =============================================================================
// Standalone Entry Point
// =============================================================================
#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_JsonStreamLite() ? 0 : 1;
}
#endif
