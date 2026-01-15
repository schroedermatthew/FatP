/**
 * @file test_DiagnosticLogger_Json.cpp
 * @brief Comprehensive unit tests for DiagnosticLogger_Json.h
 */
/*
FATP_META:
  meta_version: 1
  component: DiagnosticLogger_Json
  file_role: test
  path: tests/test_DiagnosticLogger_Json.cpp
  namespace: fat_p::testing::diagnosticlogger_json
  summary: "Unit tests for DiagnosticLogger_Json."
  related:
    docs_search: "DiagnosticLogger_Json"
    headers:
      - fat_p/JsonLite.h
      - fat_p/FatPTest.h
      - fat_p/DiagnosticLogger_Json.h
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

#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "FatPTest.h"
#include "JsonLite.h"


// ====================================================================================
// CRITICAL: DiagnosticLoggerJsonTestData MUST be defined BEFORE DiagnosticLogger_Json.h
// ====================================================================================
//
// WHY THIS ORDER MATTERS:
// -----------------------
// DiagnosticLogger_Json.h contains template function logJsonHelper<T>() which calls:
//     fat_p::to_json(j, data);  // Line 77 in DiagnosticLogger_Json.h
//
// When the compiler instantiates this template with DiagnosticLoggerJsonTestData,
// it performs Argument-Dependent Lookup (ADL) to find the correct to_json() function.
// ADL only considers functions that are VISIBLE at the point of template instantiation.
//
// WHAT BREAKS IF ORDER IS WRONG:
// -------------------------------
// If DiagnosticLoggerJsonTestData is defined AFTER #include "DiagnosticLogger_Json.h",
// then when FATP_LOG_INFO_JSON(data) is called, the compiler will:
//   1. Instantiate logJsonHelper<DiagnosticLoggerJsonTestData>
//   2. Try to find to_json(JsonValue&, const DiagnosticLoggerJsonTestData&)
//   3. FAIL because that function wasn't visible during template instantiation
//   4. Generate error C2665: no overloaded function could convert all argument types
//
// THE FIX:
// --------
// Define the struct AND its to_json/from_json functions BEFORE including
// DiagnosticLogger_Json.h. This ensures they are visible during template instantiation.
//
// REMINDER FOR FUTURE CHANGES:
// -----------------------------
// If you add more custom types for JSON logging in this test file:
//   1. Define the struct
//   2. Define to_json() and from_json()
//   3. THEN include DiagnosticLogger_Json.h (or ensure functions are visible before use)
//
// DO NOT move this block below the DiagnosticLogger_Json.h include!
// ====================================================================================

namespace fat_p
{

struct DiagnosticLoggerJsonTestData
{
    std::string name;
    int value;
    double score;
};

inline void to_json(JsonValue& j, const DiagnosticLoggerJsonTestData& data)
{
    JsonObject obj;
    obj["name"] = data.name;
    obj["value"] = static_cast<int64_t>(data.value);
    obj["score"] = data.score;
    j = std::move(obj);
}

inline void from_json(const JsonValue& j, DiagnosticLoggerJsonTestData& data)
{
    const auto& obj = std::get<JsonObject>(j);
    data.name = std::get<std::string>(obj.at("name"));
    data.value = static_cast<int>(std::get<int64_t>(obj.at("value")));
    data.score = std::get<double>(obj.at("score"));
}

} // namespace fat_p

// DiagnosticLogger_Json.h can now see DiagnosticLoggerJsonTestData and its functions
#include "DiagnosticLogger_Json.h"

namespace fat_p::testing::diagnosticlogger_json
{

using namespace fat_p::diagnostic;

class StringSink : public ISink
{
    std::vector<std::string> mMessages;
    mutable std::mutex mMutex;

public:
    void write(const LogRecord& record) override
    {
        std::lock_guard<std::mutex> lock(mMutex);
        JsonFormatter formatter;
        mMessages.push_back(formatter.format(record));
    }

    void flush() override
    {
    }

    std::vector<std::string> getMessages() const
    {
        std::lock_guard<std::mutex> lock(mMutex);
        return mMessages;
    }

    void clear()
    {
        std::lock_guard<std::mutex> lock(mMutex);
        mMessages.clear();
    }

    size_t count() const
    {
        std::lock_guard<std::mutex> lock(mMutex);
        return mMessages.size();
    }

    std::string getLast() const
    {
        std::lock_guard<std::mutex> lock(mMutex);
        return mMessages.empty() ? "" : mMessages.back();
    }
};

FATP_TEST_CASE(json_formatter_basic)
{
    JsonFormatter formatter;
    auto loc = FATP_SOURCE_LOCATION();
    LogRecord record(LogLevel::Info, "Test message", loc);

    std::string formatted = formatter.format(record);

    FATP_ASSERT_TRUE(!formatted.empty(), "Formatted string not empty");

    auto parsed = parse_json(formatted);
    FATP_ASSERT_TRUE(std::holds_alternative<JsonObject>(parsed), "Parsed as JSON object");

    const auto& obj = std::get<JsonObject>(parsed);
    FATP_ASSERT_TRUE(obj.count("timestamp"), "Contains timestamp");
    FATP_ASSERT_TRUE(obj.count("level"), "Contains level");
    FATP_ASSERT_TRUE(obj.count("message"), "Contains message");
    FATP_ASSERT_TRUE(obj.count("thread_id"), "Contains thread_id");

    std::string message = std::get<std::string>(obj.at("message"));
    FATP_ASSERT_TRUE(message == "Test message", "Message correct");

    std::string level = std::get<std::string>(obj.at("level"));
    FATP_ASSERT_TRUE(level == "INFO", "Level correct");

    return true;
}

FATP_TEST_CASE(json_formatter_with_metadata)
{
    JsonFormatter formatter;
    auto loc = FATP_SOURCE_LOCATION();

    JsonObject metaObj;
    metaObj["key"] = "value";
    metaObj["count"] = static_cast<int64_t>(42);
    std::string metadata = to_json_string(metaObj);

    LogRecord record(LogLevel::Warning, "Message with metadata", loc, metadata);

    std::string formatted = formatter.format(record);
    auto parsed = parse_json(formatted);
    const auto& obj = std::get<JsonObject>(parsed);

    FATP_ASSERT_TRUE(obj.count("data"), "Contains data field");

    const auto& dataObj = std::get<JsonObject>(obj.at("data"));
    std::string key = std::get<std::string>(dataObj.at("key"));
    int64_t count = std::get<int64_t>(dataObj.at("count"));

    FATP_ASSERT_TRUE(key == "value", "Metadata key correct");
    FATP_ASSERT_TRUE(count == 42, "Metadata count correct");

    return true;
}

FATP_TEST_CASE(json_formatter_invalid_metadata)
{
    JsonFormatter formatter;
    auto loc = FATP_SOURCE_LOCATION();

    LogRecord record(LogLevel::Error, "Message", loc, "not valid json");

    std::string formatted = formatter.format(record);
    auto parsed = parse_json(formatted);
    const auto& obj = std::get<JsonObject>(parsed);

    FATP_ASSERT_TRUE(obj.count("data_payload"), "Contains data_payload for invalid JSON");

    std::string payload = std::get<std::string>(obj.at("data_payload"));
    FATP_ASSERT_TRUE(payload == "not valid json", "Invalid JSON stored as string");

    return true;
}

FATP_TEST_CASE(json_formatter_location_info)
{
    JsonFormatter formatter;
    auto loc = FATP_SOURCE_LOCATION();
    LogRecord record(LogLevel::Debug, "Test", loc);

    std::string formatted = formatter.format(record);
    auto parsed = parse_json(formatted);
    const auto& obj = std::get<JsonObject>(parsed);

    FATP_ASSERT_TRUE(obj.count("file"), "Contains file");
    FATP_ASSERT_TRUE(obj.count("line"), "Contains line");

    std::string file = std::get<std::string>(obj.at("file"));
    int64_t line = std::get<int64_t>(obj.at("line"));

    FATP_ASSERT_TRUE(!file.empty(), "File not empty");
    FATP_ASSERT_TRUE(line > 0, "Line is positive");

    return true;
}

FATP_TEST_CASE(log_json_simple_types)
{
    getGlobalLogger().clearSinks();
    auto sink = std::make_shared<StringSink>();
    getGlobalLogger().addSink(sink);

    FATP_LOG_INFO_JSON(42);

    FATP_ASSERT_TRUE(sink->count() == 1, "One message logged");

    std::string output = sink->getLast();
    auto parsed = parse_json(output);
    const auto& obj = std::get<JsonObject>(parsed);

    FATP_ASSERT_TRUE(obj.count("data"), "Contains data");

    return true;
}

FATP_TEST_CASE(log_json_vector)
{
    getGlobalLogger().clearSinks();
    auto sink = std::make_shared<StringSink>();
    getGlobalLogger().addSink(sink);

    std::vector<int> vec = {1, 2, 3, 4, 5};
    FATP_LOG_DEBUG_JSON(vec);

    std::string output = sink->getLast();
    auto parsed = parse_json(output);
    const auto& obj = std::get<JsonObject>(parsed);
    const auto& dataArray = std::get<JsonArray>(obj.at("data"));

    FATP_ASSERT_TRUE(dataArray.size() == 5, "Array has 5 elements");
    FATP_ASSERT_TRUE(std::get<int64_t>(dataArray[0]) == 1, "First element correct");
    FATP_ASSERT_TRUE(std::get<int64_t>(dataArray[4]) == 5, "Last element correct");

    return true;
}

FATP_TEST_CASE(log_json_map)
{
    getGlobalLogger().clearSinks();
    auto sink = std::make_shared<StringSink>();
    getGlobalLogger().addSink(sink);

    std::map<std::string, int> map = {{"a", 1}, {"b", 2}, {"c", 3}};
    FATP_LOG_INFO_JSON(map);

    std::string output = sink->getLast();
    auto parsed = parse_json(output);
    const auto& obj = std::get<JsonObject>(parsed);
    const auto& dataObj = std::get<JsonObject>(obj.at("data"));

    FATP_ASSERT_TRUE(dataObj.size() == 3, "Object has 3 fields");
    FATP_ASSERT_TRUE(std::get<int64_t>(dataObj.at("a")) == 1, "Field a correct");
    FATP_ASSERT_TRUE(std::get<int64_t>(dataObj.at("b")) == 2, "Field b correct");
    FATP_ASSERT_TRUE(std::get<int64_t>(dataObj.at("c")) == 3, "Field c correct");

    return true;
}

FATP_TEST_CASE(log_json_custom_struct)
{
    getGlobalLogger().clearSinks();
    auto sink = std::make_shared<StringSink>();
    getGlobalLogger().addSink(sink);

    fat_p::DiagnosticLoggerJsonTestData data{"test", 42, 3.14};
    FATP_LOG_INFO_JSON(data);

    std::string output = sink->getLast();
    auto parsed = parse_json(output);
    const auto& obj = std::get<JsonObject>(parsed);
    const auto& dataObj = std::get<JsonObject>(obj.at("data"));

    FATP_ASSERT_TRUE(std::get<std::string>(dataObj.at("name")) == "test", "Name correct");
    FATP_ASSERT_TRUE(std::get<int64_t>(dataObj.at("value")) == 42, "Value correct");
    FATP_ASSERT_TRUE(std::abs(std::get<double>(dataObj.at("score")) - 3.14) < 0.01, "Score correct");

    return true;
}

FATP_TEST_CASE(log_with_data_string_message)
{
    getGlobalLogger().clearSinks();
    auto sink = std::make_shared<StringSink>();
    getGlobalLogger().addSink(sink);

    std::vector<int> data = {10, 20, 30};
    FATP_LOG_INFO_WITH_DATA("Array data", data);

    std::string output = sink->getLast();
    auto parsed = parse_json(output);
    const auto& obj = std::get<JsonObject>(parsed);

    FATP_ASSERT_TRUE(std::get<std::string>(obj.at("message")) == "Array data", "Message correct");

    const auto& dataArray = std::get<JsonArray>(obj.at("data"));
    FATP_ASSERT_TRUE(dataArray.size() == 3, "Data array has 3 elements");

    return true;
}

FATP_TEST_CASE(log_json_all_levels)
{
    getGlobalLogger().clearSinks();
    auto sink = std::make_shared<StringSink>();
    getGlobalLogger().addSink(sink);
    getGlobalLogger().setLevel(LogLevel::Trace);

    int value = 1;
    FATP_LOG_TRACE_JSON(value);
    FATP_LOG_DEBUG_JSON(value);
    FATP_LOG_INFO_JSON(value);
    FATP_LOG_WARNING_JSON(value);
    FATP_LOG_ERROR_JSON(value);
    FATP_LOG_FATAL_JSON(value);

    FATP_ASSERT_TRUE(sink->count() == 6, "All log levels recorded");

    auto messages = sink->getMessages();

    auto checkLevel = [](const std::string& json, const std::string& expectedLevel) {
        auto parsed = parse_json(json);
        const auto& obj = std::get<JsonObject>(parsed);
        return std::get<std::string>(obj.at("level")) == expectedLevel;
    };

    FATP_ASSERT_TRUE(checkLevel(messages[0], "TRACE"), "Trace level correct");
    FATP_ASSERT_TRUE(checkLevel(messages[1], "DEBUG"), "Debug level correct");
    FATP_ASSERT_TRUE(checkLevel(messages[2], "INFO"), "Info level correct");
    FATP_ASSERT_TRUE(checkLevel(messages[3], "WARN"), "Warning level correct");
    FATP_ASSERT_TRUE(checkLevel(messages[4], "ERROR"), "Error level correct");
    FATP_ASSERT_TRUE(checkLevel(messages[5], "FATAL"), "Fatal level correct");

    return true;
}

FATP_TEST_CASE(log_with_data_all_levels)
{
    getGlobalLogger().clearSinks();
    auto sink = std::make_shared<StringSink>();
    getGlobalLogger().addSink(sink);
    getGlobalLogger().setLevel(LogLevel::Trace);

    int data = 42;
    FATP_LOG_TRACE_WITH_DATA("Trace msg", data);
    FATP_LOG_DEBUG_WITH_DATA("Debug msg", data);
    FATP_LOG_INFO_WITH_DATA("Info msg", data);
    FATP_LOG_WARNING_WITH_DATA("Warning msg", data);
    FATP_LOG_ERROR_WITH_DATA("Error msg", data);
    FATP_LOG_FATAL_WITH_DATA("Fatal msg", data);

    FATP_ASSERT_TRUE(sink->count() == 6, "All log levels with data recorded");

    return true;
}

FATP_TEST_CASE(log_json_nested_structures)
{
    getGlobalLogger().clearSinks();
    auto sink = std::make_shared<StringSink>();
    getGlobalLogger().addSink(sink);

    std::map<std::string, std::vector<int>> nested;
    nested["first"] = {1, 2, 3};
    nested["second"] = {4, 5, 6};

    FATP_LOG_INFO_JSON(nested);

    std::string output = sink->getLast();
    auto parsed = parse_json(output);
    const auto& obj = std::get<JsonObject>(parsed);
    const auto& dataObj = std::get<JsonObject>(obj.at("data"));

    const auto& firstArray = std::get<JsonArray>(dataObj.at("first"));
    FATP_ASSERT_TRUE(firstArray.size() == 3, "First array has 3 elements");

    const auto& secondArray = std::get<JsonArray>(dataObj.at("second"));
    FATP_ASSERT_TRUE(secondArray.size() == 3, "Second array has 3 elements");

    return true;
}

FATP_TEST_CASE(log_json_empty_containers)
{
    getGlobalLogger().clearSinks();
    auto sink = std::make_shared<StringSink>();
    getGlobalLogger().addSink(sink);

    std::vector<int> emptyVec;
    FATP_LOG_INFO_JSON(emptyVec);

    std::string output = sink->getLast();
    auto parsed = parse_json(output);
    const auto& obj = std::get<JsonObject>(parsed);
    const auto& dataArray = std::get<JsonArray>(obj.at("data"));

    FATP_ASSERT_TRUE(dataArray.empty(), "Empty vector logged as empty array");

    sink->clear();

    std::map<std::string, int> emptyMap;
    FATP_LOG_INFO_JSON(emptyMap);

    output = sink->getLast();
    parsed = parse_json(output);
    const auto& obj2 = std::get<JsonObject>(parsed);
    const auto& dataObj = std::get<JsonObject>(obj2.at("data"));

    FATP_ASSERT_TRUE(dataObj.empty(), "Empty map logged as empty object");

    return true;
}


FATP_TEST_CASE(log_json_special_characters)
{
    getGlobalLogger().clearSinks();
    auto sink = std::make_shared<StringSink>();
    getGlobalLogger().addSink(sink);

    std::map<std::string, std::string> data;
    data["quotes"] = "Value with \"quotes\"";
    data["newlines"] = "Line1\nLine2";
    data["tabs"] = "Tab\there";

    FATP_LOG_INFO_JSON(data);

    std::string output = sink->getLast();
    auto parsed = parse_json(output);
    const auto& obj = std::get<JsonObject>(parsed);
    const auto& dataObj = std::get<JsonObject>(obj.at("data"));

    FATP_ASSERT_TRUE(dataObj.size() == 3, "All special char fields present");

    return true;
}

FATP_TEST_CASE(log_json_unicode)
{
    getGlobalLogger().clearSinks();
    auto sink = std::make_shared<StringSink>();
    getGlobalLogger().addSink(sink);

    std::map<std::string, std::string> data;
    // Use raw UTF-8 byte sequences to avoid C4566 warnings on Windows
    // \u4E2D\u6587 (ä¸­æ–‡) = 0xE4 0xB8 0xAD 0xE6 0x96 0x87
    // \U0001F600 (ðŸ˜€) = 0xF0 0x9F 0x98 0x80
    data["chinese"] = "\xE4\xB8\xAD\xE6\x96\x87";
    data["emoji"] = "\xF0\x9F\x98\x80";

    FATP_LOG_INFO_JSON(data);

    std::string output = sink->getLast();
    auto parsed = parse_json(output);

    FATP_ASSERT_TRUE(std::holds_alternative<JsonObject>(parsed), "Unicode JSON parsed correctly");

    return true;
}

FATP_TEST_CASE(log_json_large_data)
{
    getGlobalLogger().clearSinks();
    auto sink = std::make_shared<StringSink>();
    getGlobalLogger().addSink(sink);

    std::vector<int> largeVec;
    for (int i = 0; i < 1000; ++i)
    {
        largeVec.push_back(i);
    }

    FATP_LOG_INFO_JSON(largeVec);

    std::string output = sink->getLast();
    auto parsed = parse_json(output);
    const auto& obj = std::get<JsonObject>(parsed);
    const auto& dataArray = std::get<JsonArray>(obj.at("data"));

    FATP_ASSERT_TRUE(dataArray.size() == 1000, "Large array logged correctly");

    return true;
}

FATP_TEST_CASE(log_json_filtering)
{
    getGlobalLogger().clearSinks();
    auto sink = std::make_shared<StringSink>();
    getGlobalLogger().addSink(sink);
    getGlobalLogger().setLevel(LogLevel::Warning);

    FATP_LOG_TRACE_JSON(1);
    FATP_LOG_DEBUG_JSON(2);
    FATP_LOG_INFO_JSON(3);
    FATP_ASSERT_TRUE(sink->count() == 0, "Lower levels filtered");

    FATP_LOG_WARNING_JSON(4);
    FATP_LOG_ERROR_JSON(5);
    FATP_ASSERT_TRUE(sink->count() == 2, "Higher levels logged");

    getGlobalLogger().setLevel(LogLevel::Trace);

    return true;
}

void benchmark_json_logging()
{
    std::cout << "\n" << colors::cyan() << "DiagnosticLogger JSON Benchmarks:" << colors::reset() << "\n\n";

    getGlobalLogger().clearSinks();
    auto sink = std::make_shared<StringSink>();
    getGlobalLogger().addSink(sink);

    std::vector<int> data = {1, 2, 3, 4, 5};
    double json_time = measure_perf(
        [&data]() {
            FATP_LOG_INFO_JSON(data);
        },
        10000,
        100);
    std::cout << "JSON logging (vector): " << format_time(json_time) << "\n";

    std::map<std::string, int> mapData = {{"a", 1}, {"b", 2}, {"c", 3}};
    double map_time = measure_perf(
        [&mapData]() {
            FATP_LOG_INFO_JSON(mapData);
        },
        10000,
        100);
    std::cout << "JSON logging (map): " << format_time(map_time) << "\n";

    fat_p::DiagnosticLoggerJsonTestData customData{"test", 42, 3.14};
    double custom_time = measure_perf(
        [&customData]() {
            FATP_LOG_INFO_JSON(customData);
        },
        10000,
        100);
    std::cout << "JSON logging (custom struct): " << format_time(custom_time) << "\n";

    double with_data_time = measure_perf(
        [&data]() {
            FATP_LOG_INFO_WITH_DATA("Message", data);
        },
        10000,
        100);
    std::cout << "With data logging: " << format_time(with_data_time) << "\n";
}

} // namespace fat_p::testing::diagnosticlogger_json

namespace fat_p::testing
{

bool test_DiagnosticLogger_Json()
{
    FATP_PRINT_HEADER(DIAGNOSTIC LOGGER JSON)

    TestRunner runner;

    FATP_RUN_TEST_NS(runner, diagnosticlogger_json, json_formatter_basic);
    FATP_RUN_TEST_NS(runner, diagnosticlogger_json, json_formatter_with_metadata);
    FATP_RUN_TEST_NS(runner, diagnosticlogger_json, json_formatter_invalid_metadata);
    FATP_RUN_TEST_NS(runner, diagnosticlogger_json, json_formatter_location_info);
    FATP_RUN_TEST_NS(runner, diagnosticlogger_json, log_json_simple_types);
    FATP_RUN_TEST_NS(runner, diagnosticlogger_json, log_json_vector);
    FATP_RUN_TEST_NS(runner, diagnosticlogger_json, log_json_map);
    FATP_RUN_TEST_NS(runner, diagnosticlogger_json, log_json_custom_struct);
    FATP_RUN_TEST_NS(runner, diagnosticlogger_json, log_with_data_string_message);
    FATP_RUN_TEST_NS(runner, diagnosticlogger_json, log_json_all_levels);
    FATP_RUN_TEST_NS(runner, diagnosticlogger_json, log_with_data_all_levels);
    FATP_RUN_TEST_NS(runner, diagnosticlogger_json, log_json_nested_structures);
    FATP_RUN_TEST_NS(runner, diagnosticlogger_json, log_json_empty_containers);
    FATP_RUN_TEST_NS(runner, diagnosticlogger_json, log_json_special_characters);
    FATP_RUN_TEST_NS(runner, diagnosticlogger_json, log_json_unicode);
    FATP_RUN_TEST_NS(runner, diagnosticlogger_json, log_json_large_data);
    FATP_RUN_TEST_NS(runner, diagnosticlogger_json, log_json_filtering);

    diagnosticlogger_json::benchmark_json_logging();

    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

// =============================================================================
// Standalone Entry Point
// =============================================================================
#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_DiagnosticLogger_Json() ? 0 : 1;
}
#endif
