#include <cmath>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "JsonStreamLite.h"
#include "FatPTest.h"

#ifndef ENABLE_TEST_APPLICATION
#include "test_JsonStreamLite.h"
#endif

namespace fat_p::testing::jsonstreamlite
{

using namespace fat_p::json_stream;

// =============================================================================
// Basic Type Tests
// =============================================================================

TEST_CASE(parse_integers)
{
    JsonStreamParser parser;
    const char* json = "[0, 1, -1, 42, -42, 2147483647, -2147483648]";
    auto status = parser.feed(json, strlen(json));

    SIMPLE_ASSERT(status == ParseStatus::Done, "Parse succeeded");

    auto result = parser.take_result();
    SIMPLE_ASSERT(result.is_array(), "Is array");

    auto& arr = result.as_array();
    SIMPLE_ASSERT(arr[0].as_int() == 0, "Zero");
    SIMPLE_ASSERT(arr[1].as_int() == 1, "One");
    SIMPLE_ASSERT(arr[2].as_int() == -1, "Negative one");
    SIMPLE_ASSERT(arr[3].as_int() == 42, "42");
    SIMPLE_ASSERT(arr[4].as_int() == -42, "-42");
    SIMPLE_ASSERT(arr[5].as_int() == 2147483647, "INT_MAX");
    SIMPLE_ASSERT(arr[6].as_int() == -2147483648LL, "INT_MIN");

    return true;
}

TEST_CASE(parse_doubles)
{
    JsonStreamParser parser;
    const char* json = "[3.14, -2.5, 1.0e10, 1.5e-5, 0.0]";
    auto status = parser.feed(json, strlen(json));

    SIMPLE_ASSERT(status == ParseStatus::Done, "Parse succeeded");

    auto result = parser.take_result();
    auto& arr = result.as_array();

    SIMPLE_ASSERT(std::abs(arr[0].as_double() - 3.14) < 1e-10, "3.14");
    SIMPLE_ASSERT(std::abs(arr[1].as_double() - (-2.5)) < 1e-10, "-2.5");
    SIMPLE_ASSERT(std::abs(arr[2].as_double() - 1.0e10) < 1e5, "1e10");
    SIMPLE_ASSERT(std::abs(arr[3].as_double() - 1.5e-5) < 1e-10, "1.5e-5");
    SIMPLE_ASSERT(arr[4].as_double() == 0.0, "0.0");

    return true;
}

TEST_CASE(parse_strings)
{
    JsonStreamParser parser;
    const char* json = R"(["hello", "", "with spaces", "unicode: \u0041"])";
    auto status = parser.feed(json, strlen(json));

    SIMPLE_ASSERT(status == ParseStatus::Done, "Parse succeeded");

    auto result = parser.take_result();
    auto& arr = result.as_array();

    SIMPLE_ASSERT(arr[0].as_string() == "hello", "hello");
    SIMPLE_ASSERT(arr[1].as_string() == "", "empty");
    SIMPLE_ASSERT(arr[2].as_string() == "with spaces", "spaces");
    SIMPLE_ASSERT(arr[3].as_string() == "unicode: A", "unicode");

    return true;
}

TEST_CASE(parse_escape_sequences)
{
    JsonStreamParser parser;
    const char* json = R"(["a\nb", "a\tb", "a\"b", "a\\b", "a\/b", "a\rb", "a\fb", "a\bb"])";
    auto status = parser.feed(json, strlen(json));

    SIMPLE_ASSERT(status == ParseStatus::Done, "Parse succeeded");

    auto result = parser.take_result();
    auto& arr = result.as_array();

    SIMPLE_ASSERT(arr[0].as_string() == "a\nb", "newline");
    SIMPLE_ASSERT(arr[1].as_string() == "a\tb", "tab");
    SIMPLE_ASSERT(arr[2].as_string() == "a\"b", "quote");
    SIMPLE_ASSERT(arr[3].as_string() == "a\\b", "backslash");
    SIMPLE_ASSERT(arr[4].as_string() == "a/b", "slash");
    SIMPLE_ASSERT(arr[5].as_string() == "a\rb", "carriage return");
    SIMPLE_ASSERT(arr[6].as_string() == "a\fb", "form feed");
    SIMPLE_ASSERT(arr[7].as_string() == "a\bb", "backspace");

    return true;
}

TEST_CASE(parse_literals)
{
    JsonStreamParser parser;
    const char* json = "[true, false, null]";
    auto status = parser.feed(json, strlen(json));

    SIMPLE_ASSERT(status == ParseStatus::Done, "Parse succeeded");

    auto result = parser.take_result();
    auto& arr = result.as_array();

    SIMPLE_ASSERT(arr[0].is_bool() && arr[0].as_bool() == true, "true");
    SIMPLE_ASSERT(arr[1].is_bool() && arr[1].as_bool() == false, "false");
    SIMPLE_ASSERT(arr[2].is_null(), "null");

    return true;
}

// =============================================================================
// Container Tests
// =============================================================================

TEST_CASE(parse_empty_containers)
{
    JsonStreamParser parser1;
    auto s1 = parser1.feed("[]", 2);
    SIMPLE_ASSERT(s1 == ParseStatus::Done, "Empty array");
    SIMPLE_ASSERT(parser1.result().is_array(), "Is array");
    SIMPLE_ASSERT(parser1.result().as_array().empty(), "Array empty");

    JsonStreamParser parser2;
    auto s2 = parser2.feed("{}", 2);
    SIMPLE_ASSERT(s2 == ParseStatus::Done, "Empty object");
    SIMPLE_ASSERT(parser2.result().is_object(), "Is object");
    SIMPLE_ASSERT(parser2.result().as_object().empty(), "Object empty");

    return true;
}

TEST_CASE(parse_simple_array)
{
    JsonStreamParser parser;
    const char* json = "[1, 2, 3]";
    auto status = parser.feed(json, strlen(json));

    SIMPLE_ASSERT(status == ParseStatus::Done, "Parse succeeded");

    auto result = parser.take_result();
    SIMPLE_ASSERT(result.is_array(), "Is array");

    auto& arr = result.as_array();
    SIMPLE_ASSERT(arr.size() == 3, "Size 3");
    SIMPLE_ASSERT(arr[0].as_int() == 1, "First");
    SIMPLE_ASSERT(arr[1].as_int() == 2, "Second");
    SIMPLE_ASSERT(arr[2].as_int() == 3, "Third");

    return true;
}

TEST_CASE(parse_simple_object)
{
    JsonStreamParser parser;
    const char* json = R"({"name": "test", "value": 42})";
    auto status = parser.feed(json, strlen(json));

    SIMPLE_ASSERT(status == ParseStatus::Done, "Parse succeeded");

    auto result = parser.take_result();
    SIMPLE_ASSERT(result.is_object(), "Is object");

    auto& obj = result.as_object();
    SIMPLE_ASSERT(obj.at("name").as_string() == "test", "name");
    SIMPLE_ASSERT(obj.at("value").as_int() == 42, "value");

    return true;
}

TEST_CASE(parse_nested_arrays)
{
    JsonStreamParser parser;
    const char* json = "[[1, 2], [3, 4], [5, 6]]";
    auto status = parser.feed(json, strlen(json));

    SIMPLE_ASSERT(status == ParseStatus::Done, "Parse succeeded");

    auto result = parser.take_result();
    SIMPLE_ASSERT(result.as_array().size() == 3, "Outer size");
    SIMPLE_ASSERT(result.as_array()[0].as_array()[0].as_int() == 1, "First");
    SIMPLE_ASSERT(result.as_array()[2].as_array()[1].as_int() == 6, "Last");

    return true;
}

TEST_CASE(parse_nested_objects)
{
    JsonStreamParser parser;
    const char* json = R"({"outer": {"inner": {"deep": 99}}})";
    auto status = parser.feed(json, strlen(json));

    SIMPLE_ASSERT(status == ParseStatus::Done, "Parse succeeded");

    auto result = parser.take_result();
    int val = result.as_object()
                    .at("outer").as_object()
                    .at("inner").as_object()
                    .at("deep").as_int();
    SIMPLE_ASSERT(val == 99, "Deep value");

    return true;
}

TEST_CASE(parse_mixed_nesting)
{
    JsonStreamParser parser;
    const char* json = R"({"items": [{"id": 1}, {"id": 2}], "count": 2})";
    auto status = parser.feed(json, strlen(json));

    SIMPLE_ASSERT(status == ParseStatus::Done, "Parse succeeded");

    auto result = parser.take_result();
    auto& items = result.as_object().at("items").as_array();

    SIMPLE_ASSERT(items.size() == 2, "Items size");
    SIMPLE_ASSERT(items[0].as_object().at("id").as_int() == 1, "First id");
    SIMPLE_ASSERT(items[1].as_object().at("id").as_int() == 2, "Second id");
    SIMPLE_ASSERT(result.as_object().at("count").as_int() == 2, "Count");

    return true;
}

// =============================================================================
// Streaming Tests
// =============================================================================

TEST_CASE(chunked_input_byte_by_byte)
{
    JsonStreamParser parser;
    const char* json = R"({"key": [1, 2, 3]})";
    std::size_t len = strlen(json);

    ParseStatus status = ParseStatus::NeedMoreData;
    for (std::size_t i = 0; i < len; ++i)
    {
        status = parser.feed(json + i, 1);
        SIMPLE_ASSERT(status != ParseStatus::Error, "No error at byte");
    }

    SIMPLE_ASSERT(status == ParseStatus::Done, "Done");
    SIMPLE_ASSERT(parser.result().as_object().at("key").as_array().size() == 3, "Array");

    return true;
}

TEST_CASE(chunked_input_random_sizes)
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
        SIMPLE_ASSERT(status != ParseStatus::Error, "No error");

        pos += chunk;
        ++chunk_idx;
    }

    SIMPLE_ASSERT(status == ParseStatus::Done, "Done");

    return true;
}

// =============================================================================
// Limit Tests
// =============================================================================

TEST_CASE(limit_max_depth)
{
    JsonStreamParser parser;
    parser.set_max_depth(3);

    // Create 5-level deep array
    const char* json = "[[[[[1]]]]]";
    auto status = parser.feed(json, strlen(json));

    SIMPLE_ASSERT(status == ParseStatus::Error, "Error");
    SIMPLE_ASSERT(parser.error() == ParseError::MaxDepthExceeded, "Depth error");

    return true;
}

TEST_CASE(limit_string_size)
{
    JsonStreamParser parser;
    parser.set_max_string_size(10);

    std::string json = "[\"" + std::string(20, 'x') + "\"]";
    auto status = parser.feed(json.data(), json.size());

    SIMPLE_ASSERT(status == ParseStatus::Error, "Error");
    SIMPLE_ASSERT(parser.error() == ParseError::MaxStringSizeExceeded, "String error");

    return true;
}

TEST_CASE(limit_total_size)
{
    JsonStreamParser parser;
    parser.set_max_total_size(20);

    const char* json = R"({"key": "this is a very long value"})";
    auto status = parser.feed(json, strlen(json));

    SIMPLE_ASSERT(status == ParseStatus::Error, "Error");
    SIMPLE_ASSERT(parser.error() == ParseError::MaxTotalSizeExceeded, "Total error");

    return true;
}

// =============================================================================
// Error Tests
// =============================================================================

TEST_CASE(error_invalid_json)
{
    JsonStreamParser parser1;
    auto s1 = parser1.feed("invalid", 7);
    SIMPLE_ASSERT(s1 == ParseStatus::Error, "Invalid literal");

    JsonStreamParser parser2;
    auto s2 = parser2.feed("[1,]", 4);
    SIMPLE_ASSERT(s2 == ParseStatus::Error, "Trailing comma in array");

    JsonStreamParser parser3;
    auto s3 = parser3.feed("{\"key\":}", 8);
    SIMPLE_ASSERT(s3 == ParseStatus::Error, "Missing value");

    return true;
}

TEST_CASE(error_invalid_number)
{
    JsonStreamParser parser1;
    auto s1 = parser1.feed("[01]", 4);
    SIMPLE_ASSERT(s1 == ParseStatus::Error, "Leading zero");
    SIMPLE_ASSERT(parser1.error() == ParseError::InvalidNumber, "Number error");

    return true;
}

TEST_CASE(error_invalid_escape)
{
    JsonStreamParser parser;
    auto status = parser.feed(R"(["test\q"])", 10);
    SIMPLE_ASSERT(status == ParseStatus::Error, "Invalid escape");
    SIMPLE_ASSERT(parser.error() == ParseError::InvalidEscapeSequence, "Escape error");

    return true;
}

TEST_CASE(error_truncated_input)
{
    JsonStreamParser parser;
    auto status = parser.feed("[1, 2", 5);
    SIMPLE_ASSERT(status == ParseStatus::NeedMoreData, "Need more data");
    SIMPLE_ASSERT(!parser.is_done(), "Not done");

    return true;
}

// =============================================================================
// Statistics Tests
// =============================================================================

TEST_CASE(statistics_tracking)
{
    JsonStreamParser parser;
    const char* json = R"({"a": [1, 2], "b": {"c": 3}})";
    auto status = parser.feed(json, strlen(json));

    SIMPLE_ASSERT(status == ParseStatus::Done, "Done");

    const auto& stats = parser.stats();
    SIMPLE_ASSERT(stats.bytes_consumed == strlen(json), "Bytes");
    SIMPLE_ASSERT(stats.max_depth_seen >= 2, "Max depth");
    SIMPLE_ASSERT(stats.values_parsed > 0, "Values parsed");

    return true;
}

// =============================================================================
// Convenience Function Tests
// =============================================================================

TEST_CASE(parse_json_convenience)
{
    auto result = parse_json("[1, 2, 3]");
    SIMPLE_ASSERT(result.is_array(), "Is array");
    SIMPLE_ASSERT(result.as_array().size() == 3, "Size");

    std::string json_str = R"({"key": "value"})";
    auto result2 = parse_json(json_str);
    SIMPLE_ASSERT(result2.as_object().at("key").as_string() == "value", "Value");

    return true;
}

TEST_CASE(using_macro)
{
    USING_JSON_STREAM_LITE();

    auto result = parse_json("[true, false]");
    SIMPLE_ASSERT(result.is_array(), "Is array");
    SIMPLE_ASSERT(result.as_array()[0].as_bool() == true, "True");

    return true;
}

// =============================================================================
// Whitespace Tests
// =============================================================================

TEST_CASE(parse_with_whitespace)
{
    JsonStreamParser parser;
    const char* json = R"(
        {
            "key" : "value" ,
            "array" : [ 1 , 2 , 3 ]
        }
    )";
    auto status = parser.feed(json, strlen(json));

    SIMPLE_ASSERT(status == ParseStatus::Done, "Done");
    SIMPLE_ASSERT(parser.result().as_object().at("key").as_string() == "value", "Key");

    return true;
}

// =============================================================================
// Benchmarks
// =============================================================================

void benchmark_parsing()
{
    std::cout << "\n" << colors::cyan() << "JsonStreamLite Benchmarks:"
              << colors::reset() << "\n\n";

    constexpr int iterations = 1000;
    constexpr int warmup = 100;

    // Build test JSON
    std::string json = "[";
    for (int i = 0; i < 100; ++i)
    {
        if (i > 0)
        {
            json += ",";
        }
        json += R"({"id":)" + std::to_string(i);
        json += R"(,"name":"item_)" + std::to_string(i) + "\"";
        json += R"(,"value":)" + std::to_string(i * 100) + "}";
    }
    json += "]";

    std::cout << "Test data size: " << json.size() << " bytes\n\n";

    // Full buffer parse
    double time_full = measure_perf([&]()
    {
        JsonStreamParser parser;
        auto status = parser.feed(json.data(), json.size());
        DoNotOptimize(status);
    }, iterations, warmup);

    std::cout << "Full buffer parse:    " << format_time(time_full) << "\n";

    // Byte-at-a-time parse
    double time_byte = measure_perf([&]()
    {
        JsonStreamParser parser;
        for (std::size_t i = 0; i < json.size(); ++i)
        {
            auto status = parser.feed(json.data() + i, 1);
            DoNotOptimize(status);
        }
    }, iterations, warmup);

    std::cout << "Byte-at-a-time parse: " << format_time(time_byte) << "\n";

    // Calculate throughput
    double throughput_full = json.size() / time_full / 1000.0;
    double throughput_byte = json.size() / time_byte / 1000.0;

    std::cout << "\nThroughput (full):    " << std::fixed << std::setprecision(1)
              << throughput_full << " MB/s\n";
    std::cout << "Throughput (byte):    " << std::fixed << std::setprecision(1)
              << throughput_byte << " MB/s\n";
}

} // namespace fat_p::testing::jsonstreamlite

namespace fat_p::testing
{

bool test_JsonStreamLite()
{
    PRINT_HEADER(JSON STREAM LITE)

    TestRunner runner;

    RUN_TEST_NS(runner, jsonstreamlite, parse_integers);
    RUN_TEST_NS(runner, jsonstreamlite, parse_doubles);
    RUN_TEST_NS(runner, jsonstreamlite, parse_strings);
    RUN_TEST_NS(runner, jsonstreamlite, parse_escape_sequences);
    RUN_TEST_NS(runner, jsonstreamlite, parse_literals);
    RUN_TEST_NS(runner, jsonstreamlite, parse_empty_containers);
    RUN_TEST_NS(runner, jsonstreamlite, parse_simple_array);
    RUN_TEST_NS(runner, jsonstreamlite, parse_simple_object);
    RUN_TEST_NS(runner, jsonstreamlite, parse_nested_arrays);
    RUN_TEST_NS(runner, jsonstreamlite, parse_nested_objects);
    RUN_TEST_NS(runner, jsonstreamlite, parse_mixed_nesting);
    RUN_TEST_NS(runner, jsonstreamlite, chunked_input_byte_by_byte);
    RUN_TEST_NS(runner, jsonstreamlite, chunked_input_random_sizes);
    RUN_TEST_NS(runner, jsonstreamlite, limit_max_depth);
    RUN_TEST_NS(runner, jsonstreamlite, limit_string_size);
    RUN_TEST_NS(runner, jsonstreamlite, limit_total_size);
    RUN_TEST_NS(runner, jsonstreamlite, error_invalid_json);
    RUN_TEST_NS(runner, jsonstreamlite, error_invalid_number);
    RUN_TEST_NS(runner, jsonstreamlite, error_invalid_escape);
    RUN_TEST_NS(runner, jsonstreamlite, error_truncated_input);
    RUN_TEST_NS(runner, jsonstreamlite, statistics_tracking);
    RUN_TEST_NS(runner, jsonstreamlite, parse_json_convenience);
    RUN_TEST_NS(runner, jsonstreamlite, using_macro);
    RUN_TEST_NS(runner, jsonstreamlite, parse_with_whitespace);

    jsonstreamlite::benchmark_parsing();

    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_JsonStreamLite() ? 0 : 1;
}
#endif
