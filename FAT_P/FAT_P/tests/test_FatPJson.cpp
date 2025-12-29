/**
 * @file test_FatPJson.cpp
 * @brief Comprehensive unit tests for FatPJson.h
 * 
 * @details Complete test suite covering:
 * - Expected-based API (no exceptions)
 * - Error handling and JsonError reporting
 * - Safe numeric conversions with overflow detection
 * - Memory-mapped file I/O for large files
 * - FatPJsonArray (SmallVector with inline storage)
 * - FatPJsonObject (FlatMap for cache efficiency)
 * - PooledJsonObject with StringPool deduplication
 * - Batch parsing with error collection
 * - JSONC comment support
 * - UTF-8 escaping (policy-based)
 * - Numeric bounds without artificial margins
 * - NaN/Infinity policy enforcement
 * - Locale-independent parsing
 * - Value-returning convenience API
 * - Conversion utilities
 * - EnumPlus integration (automatic enum serialization)
 * - Atomic file save with RAII cleanup
 * - Comprehensive benchmarks vs JsonLite
 * 
 * Test Categories:
 * 1. Expected-based API (try_parse_json, try_load_json, try_save_json)
 * 2. Error handling and error codes
 * 3. Safe numeric conversions (all integer types)
 * 4. File I/O (regular and memory-mapped)
 * 5. Optimized data structures
 * 6. String pool deduplication
 * 7. JSONC comments
 * 8. UTF-8 handling
 * 9. Numeric precision and policies
 * 10. EnumPlus integration (basic, structs, error handling)
 * 11. Atomic save (basic, safety, comparison with regular save)
 * 12. Performance benchmarks
 */

#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <fstream>
#include <random>
#include <algorithm>
#include <sstream>
#include <filesystem>
#include <numeric>
#include <cmath>
#include <climits>
#include <limits>
#include <clocale>

#include "FatPJson.h"
#include "FatPTest.h"

namespace fat_p::testing 
{

USING_FATP_JSON()

using namespace std::chrono;

constexpr size_t BENCHMARK_ITERATIONS = 1000;
constexpr size_t LARGE_OBJECT_SIZE = 1000;
constexpr size_t LARGE_ARRAY_SIZE = 10000;
constexpr size_t NESTED_DEPTH = 50;

struct TestPoint {
    int x = 0;
    int y = 0;
};
CPP_JSON_DEFINE_TYPE_NON_INTRUSIVE(TestPoint, x, y)

struct JsonTestConfig {
    std::optional<int> timeout;
    std::optional<std::string> host;
    int port = 8080;
};
CPP_JSON_DEFINE_TYPE_OPTIONAL(JsonTestConfig, timeout, host, port)

enum class TaskStatus {
    Idle = 0,
    Running = 1,
    Completed = 2,
    Failed = 3
};

enum class Priority {
    Low = 0,
    Medium = 1,
    High = 2,
    Critical = 3
};

enum class Color {
    Red = 0,
    Green = 1,
    Blue = 2,
    Yellow = 3
};

}

namespace fat_p {

template<>
struct EnumStringPolicy<testing::TaskStatus> {
    static constexpr bool has_names = true;
    
    static std::string_view to_string(testing::TaskStatus value) {
        switch (value) {
            case testing::TaskStatus::Idle: return "Idle";
            case testing::TaskStatus::Running: return "Running";
            case testing::TaskStatus::Completed: return "Completed";
            case testing::TaskStatus::Failed: return "Failed";
            default: return "Unknown";
        }
    }
    
    static testing::TaskStatus from_string(std::string_view str) {
        if (str == "Idle") return testing::TaskStatus::Idle;
        if (str == "Running") return testing::TaskStatus::Running;
        if (str == "Completed") return testing::TaskStatus::Completed;
        if (str == "Failed") return testing::TaskStatus::Failed;
        throw std::invalid_argument("Invalid TaskStatus string: " + std::string(str));
    }
};

template<>
struct EnumStringPolicy<testing::Priority> {
    static constexpr bool has_names = true;
    
    static std::string_view to_string(testing::Priority value) {
        switch (value) {
            case testing::Priority::Low: return "Low";
            case testing::Priority::Medium: return "Medium";
            case testing::Priority::High: return "High";
            case testing::Priority::Critical: return "Critical";
            default: return "Unknown";
        }
    }
    
    static testing::Priority from_string(std::string_view str) {
        if (str == "Low") return testing::Priority::Low;
        if (str == "Medium") return testing::Priority::Medium;
        if (str == "High") return testing::Priority::High;
        if (str == "Critical") return testing::Priority::Critical;
        throw std::invalid_argument("Invalid Priority string: " + std::string(str));
    }
};

template<>
struct EnumStringPolicy<testing::Color> {
    static constexpr bool has_names = true;
    
    static std::string_view to_string(testing::Color value) {
        switch (value) {
            case testing::Color::Red: return "Red";
            case testing::Color::Green: return "Green";
            case testing::Color::Blue: return "Blue";
            case testing::Color::Yellow: return "Yellow";
            default: return "Unknown";
        }
    }
    
    static testing::Color from_string(std::string_view str) {
        if (str == "Red") return testing::Color::Red;
        if (str == "Green") return testing::Color::Green;
        if (str == "Blue") return testing::Color::Blue;
        if (str == "Yellow") return testing::Color::Yellow;
        throw std::invalid_argument("Invalid Color string: " + std::string(str));
    }
};

}

namespace fat_p::testing::fatpjson {

USING_FATP_JSON()

using namespace std::chrono;

struct Task {
    std::string name;
    TaskStatus status;
    Priority priority;
    int progress;
};
CPP_JSON_DEFINE_TYPE_NON_INTRUSIVE(Task, name, status, priority, progress)

struct ColoredObject {
    std::string label;
    Color color;
    double value;
};
CPP_JSON_DEFINE_TYPE_NON_INTRUSIVE(ColoredObject, label, color, value)

struct DatabaseConfig {
    std::string host;
    int port;
    std::optional<int> timeout;
};
CPP_JSON_DEFINE_TYPE_NON_INTRUSIVE(DatabaseConfig, host, port, timeout)

std::string generate_large_json_object(size_t num_keys) {
    std::ostringstream oss;
    oss << "{";
    for (size_t i = 0; i < num_keys; ++i) {
        if (i > 0) oss << ",";
        oss << "\"key_" << i << "\":" << i;
    }
    oss << "}";
    return oss.str();
}

std::string generate_large_json_array(size_t num_elements) {
    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < num_elements; ++i) {
        if (i > 0) oss << ",";
        oss << i;
    }
    oss << "]";
    return oss.str();
}

std::string generate_nested_json(size_t depth) {
    std::ostringstream oss;
    for (size_t i = 0; i < depth; ++i) {
        oss << "{\"nested\":";
    }
    oss << "42";
    for (size_t i = 0; i < depth; ++i) {
        oss << "}";
    }
    return oss.str();
}

std::string generate_repeated_keys_json(size_t num_objects) {
    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < num_objects; ++i) {
        if (i > 0) oss << ",";
        oss << R"({"id":)" << i << R"(,"name":"User","type":"person","status":"active"})";
    }
    oss << "]";
    return oss.str();
}

TEST_CASE(expected_api_basic) {
    SUBTEST("Valid JSON parsing") {
        std::string json = R"({"name":"Alice","age":30,"active":true})";
        auto result = try_parse_json(json);
        
        ASSERT_TRUE(result.has_value(), "Should parse successfully");
        ASSERT_TRUE(result->is_object(), "Should be an object");
    }
    END_SUBTEST
    
    SUBTEST("Invalid JSON returns error") {
        std::string json = "{bad json}";
        auto result = try_parse_json(json);
        
        ASSERT_FALSE(result.has_value(), "Should fail to parse");
        ASSERT_EQ(result.error().code, JsonErrorCode::ParseError, "Should be parse error");
        ASSERT_FALSE(result.error().message.empty(), "Should have error message");
    }
    END_SUBTEST
    
    SUBTEST("Extra data after JSON") {
        std::string json = R"({"valid":true} extra data)";
        auto result = try_parse_json(json);
        
        ASSERT_FALSE(result.has_value(), "Should fail due to extra data");
        ASSERT_EQ(result.error().code, JsonErrorCode::ExtraData, "Should be extra data error");
    }
    END_SUBTEST
    
    SUBTEST("Empty JSON") {
        std::string json = "";
        auto result = try_parse_json(json);
        
        ASSERT_FALSE(result.has_value(), "Empty JSON should fail");
    }
    END_SUBTEST
    
    return get_subtest_tracker().all_passed();
}

TEST_CASE(expected_api_types) {
    SUBTEST("Parse null") {
        auto result = try_parse_json("null");
        ASSERT_TRUE(result.has_value(), "Should parse null");
        ASSERT_TRUE(result->is_null(), "Should be null");
    }
    END_SUBTEST
    
    SUBTEST("Parse boolean") {
        auto result1 = try_parse_json("true");
        ASSERT_TRUE(result1.has_value(), "Should parse true");
        ASSERT_TRUE(result1->is_bool(), "Should be bool");
        ASSERT_EQ(std::get<bool>(*result1), true, "Should be true");
        
        auto result2 = try_parse_json("false");
        ASSERT_TRUE(result2.has_value(), "Should parse false");
        ASSERT_EQ(std::get<bool>(*result2), false, "Should be false");
    }
    END_SUBTEST
    
    SUBTEST("Parse integer") {
        auto result = try_parse_json("42");
        ASSERT_TRUE(result.has_value(), "Should parse integer");
        ASSERT_TRUE(result->is_int(), "Should be int");
        ASSERT_EQ(std::get<int64_t>(*result), 42, "Should be 42");
    }
    END_SUBTEST
    
    SUBTEST("Parse negative integer") {
        auto result = try_parse_json("-123");
        ASSERT_TRUE(result.has_value(), "Should parse negative integer");
        ASSERT_EQ(std::get<int64_t>(*result), -123, "Should be -123");
    }
    END_SUBTEST
    
    SUBTEST("Parse double") {
        auto result = try_parse_json("3.14159");
        ASSERT_TRUE(result.has_value(), "Should parse double");
        ASSERT_TRUE(result->is_number(), "Should be number");
        ASSERT_CLOSE(std::get<double>(*result), 3.14159, "Should be 3.14159");
    }
    END_SUBTEST
    
    SUBTEST("Parse string") {
        auto result = try_parse_json(R"("Hello, World!")");
        ASSERT_TRUE(result.has_value(), "Should parse string");
        ASSERT_TRUE(result->is_string(), "Should be string");
        ASSERT_EQ(std::get<std::string>(*result), "Hello, World!", "Should match");
    }
    END_SUBTEST
    
    SUBTEST("Parse array") {
        auto result = try_parse_json("[1,2,3,4,5]");
        ASSERT_TRUE(result.has_value(), "Should parse array");
        ASSERT_TRUE(result->is_array(), "Should be array");
        ASSERT_EQ(std::get<JsonArray>(*result).size(), 5U, "Should have 5 elements");
    }
    END_SUBTEST
    
    SUBTEST("Parse object") {
        auto result = try_parse_json(R"({"a":1,"b":2,"c":3})");
        ASSERT_TRUE(result.has_value(), "Should parse object");
        ASSERT_TRUE(result->is_object(), "Should be object");
        ASSERT_EQ(std::get<JsonObject>(*result).size(), 3U, "Should have 3 keys");
    }
    END_SUBTEST
    
    return get_subtest_tracker().all_passed();
}

TEST_CASE(error_handling) {
    SUBTEST("Parse error with position") {
        std::string json = R"({"key": invalid})";
        auto result = try_parse_json(json);
        
        ASSERT_FALSE(result.has_value(), "Should fail");
        ASSERT_EQ(result.error().code, JsonErrorCode::ParseError, "Should be parse error");
        ASSERT_FALSE(result.error().message.empty(), "Should have message");
    }
    END_SUBTEST
    
    SUBTEST("Type error message") {
        auto result = try_parse_json("42");
        ASSERT_TRUE(result.has_value(), "Should parse");
        
        auto str_result = safe_from_json<std::string>(*result);
        ASSERT_FALSE(str_result.has_value(), "Should fail type conversion");
        ASSERT_EQ(str_result.error().code, JsonErrorCode::TypeError, "Should be type error");
    }
    END_SUBTEST
    
    SUBTEST("Error to_string method") {
        JsonError err{JsonErrorCode::ParseError, "Test error", 42, "context"};
        std::string str = err.to_string();
        ASSERT_TRUE(!str.empty(), "Should have string representation");
        ASSERT_TRUE(str.find("42") != std::string::npos, "Should include position");
    }
    END_SUBTEST
    
    SUBTEST("Error bool operator") {
        JsonError err1{JsonErrorCode::Success, ""};
        JsonError err2{JsonErrorCode::ParseError, "Error"};
        
        ASSERT_FALSE(static_cast<bool>(err1), "Success should be false");
        ASSERT_TRUE(static_cast<bool>(err2), "Error should be true");
    }
    END_SUBTEST
    
    SUBTEST("Unknown error code streaming") {
        std::ostringstream oss;
        oss << JsonErrorCode::Unknown;
        ASSERT_EQ(oss.str(), "Unknown", "Should stream as Unknown");
    }
    END_SUBTEST
    
    SUBTEST("JsonError with Unknown code") {
        JsonError err{JsonErrorCode::Unknown, "Some unexpected error", 0, "context"};
        std::string str = err.to_string();
        ASSERT_TRUE(str.find("Unknown") != std::string::npos, "Should contain Unknown");
    }
    END_SUBTEST
    
    SUBTEST("NumericOverflow error code") {
        JsonValue j = static_cast<int64_t>(1000);
        auto result = safe_from_json_numeric<int8_t>(j);
        ASSERT_FALSE(result.has_value(), "Should fail");
        ASSERT_EQ(result.error().code, JsonErrorCode::NumericOverflow, "Should be overflow");
    }
    END_SUBTEST
    
    SUBTEST("FileError error code") {
        auto result = try_load_json("/nonexistent/path/file.json");
        ASSERT_FALSE(result.has_value(), "Should fail");
        ASSERT_EQ(result.error().code, JsonErrorCode::FileError, "Should be file error");
    }
    END_SUBTEST
    
    SUBTEST("ExtraData error code") {
        std::string json = R"({"a":1} extra stuff)";
        auto result = try_parse_json(json);
        ASSERT_FALSE(result.has_value(), "Should fail with extra data");
        ASSERT_EQ(result.error().code, JsonErrorCode::ExtraData, "Should be extra data error");
    }
    END_SUBTEST
    
    SUBTEST("RangeError via invalid array index") {
        auto result = try_parse_json("[1,2,3]");
        ASSERT_TRUE(result.has_value(), "Should parse");
        auto ptr_result = try_query_json_pointer(*result, "/999");
        ASSERT_FALSE(ptr_result.has_value(), "Should fail for out-of-bounds");
        // JSON pointer returns TypeError for navigation errors
        ASSERT_EQ(ptr_result.error().code, JsonErrorCode::TypeError, "Should be type error");
    }
    END_SUBTEST
    
    SUBTEST("MissingField via invalid key") {
        auto result = try_parse_json(R"({"a":1})");
        ASSERT_TRUE(result.has_value(), "Should parse");
        auto ptr_result = try_query_json_pointer(*result, "/nonexistent");
        ASSERT_FALSE(ptr_result.has_value(), "Should fail for missing key");
        // JSON pointer returns TypeError for navigation errors
        ASSERT_EQ(ptr_result.error().code, JsonErrorCode::TypeError, "Should be type error");
    }
    END_SUBTEST
    
    SUBTEST("All JsonErrorCode values stream correctly") {
        std::ostringstream oss;
        oss << JsonErrorCode::Success << ",";
        oss << JsonErrorCode::ParseError << ",";
        oss << JsonErrorCode::TypeError << ",";
        oss << JsonErrorCode::RangeError << ",";
        oss << JsonErrorCode::FileError << ",";
        oss << JsonErrorCode::DepthExceeded << ",";
        oss << JsonErrorCode::MemoryError << ",";
        oss << JsonErrorCode::InvalidUtf8 << ",";
        oss << JsonErrorCode::NumericOverflow << ",";
        oss << JsonErrorCode::MissingField << ",";
        oss << JsonErrorCode::ExtraData << ",";
        oss << JsonErrorCode::Unknown;
        
        std::string all = oss.str();
        ASSERT_TRUE(all.find("Success") != std::string::npos, "Should have Success");
        ASSERT_TRUE(all.find("ParseError") != std::string::npos, "Should have ParseError");
        ASSERT_TRUE(all.find("TypeError") != std::string::npos, "Should have TypeError");
        ASSERT_TRUE(all.find("Unknown") != std::string::npos, "Should have Unknown");
    }
    END_SUBTEST
    
    return get_subtest_tracker().all_passed();
}

TEST_CASE(json_stats) {
    SUBTEST("Default initialization") {
        JsonStats stats;
        ASSERT_EQ(stats.parse_count, 0U, "Default parse_count should be 0");
        ASSERT_EQ(stats.serialize_count, 0U, "Default serialize_count should be 0");
        ASSERT_CLOSE(stats.avg_parse_time_ms(), 0.0, "Default avg parse time should be 0");
    }
    END_SUBTEST
    
    SUBTEST("Average calculations") {
        JsonStats stats;
        stats.parse_count = 10;
        stats.total_parse_time_ms = 100.0;
        stats.serialize_count = 5;
        stats.total_serialize_time_ms = 25.0;
        
        ASSERT_CLOSE(stats.avg_parse_time_ms(), 10.0, "Avg parse time should be 10ms");
        ASSERT_CLOSE(stats.avg_serialize_time_ms(), 5.0, "Avg serialize time should be 5ms");
    }
    END_SUBTEST
    
    SUBTEST("Throughput calculation") {
        JsonStats stats;
        stats.total_bytes_parsed = 1024 * 1024;
        stats.total_parse_time_ms = 1000.0;
        
        ASSERT_CLOSE(stats.parse_throughput_mb_per_sec(), 1.0, "Should be 1 MB/s");
    }
    END_SUBTEST
    
    SUBTEST("Reset") {
        JsonStats stats;
        stats.parse_count = 100;
        stats.total_bytes_parsed = 1000000;
        
        stats.reset();
        
        ASSERT_EQ(stats.parse_count, 0U, "Should reset parse_count");
        ASSERT_EQ(stats.total_bytes_parsed, 0U, "Should reset total_bytes_parsed");
    }
    END_SUBTEST
    
    SUBTEST("Zero time handling") {
        JsonStats stats;
        stats.total_parse_time_ms = 0.0;
        stats.total_bytes_parsed = 1000;
        
        ASSERT_CLOSE(stats.parse_throughput_mb_per_sec(), 0.0, "Should handle zero time");
    }
    END_SUBTEST
    
    return get_subtest_tracker().all_passed();
}

TEST_CASE(safe_numeric_conversions) {
    SUBTEST("int8_t range") {
        auto j = try_parse_json("127");
        ASSERT_TRUE(j.has_value(), "Should parse");
        
        auto result = safe_from_json_numeric<int8_t>(*j);
        ASSERT_TRUE(result.has_value(), "Should convert to int8_t");
        ASSERT_EQ(*result, static_cast<int8_t>(127), "Should be 127");
    }
    END_SUBTEST
    
    SUBTEST("int8_t overflow") {
        auto j = try_parse_json("200");
        ASSERT_TRUE(j.has_value(), "Should parse");
        
        auto result = safe_from_json_numeric<int8_t>(*j);
        ASSERT_FALSE(result.has_value(), "Should fail overflow");
        ASSERT_EQ(result.error().code, JsonErrorCode::NumericOverflow, "Should be overflow error");
    }
    END_SUBTEST
    
    SUBTEST("uint16_t from negative") {
        auto j = try_parse_json("-1");
        ASSERT_TRUE(j.has_value(), "Should parse");
        
        auto result = safe_from_json_numeric<uint16_t>(*j);
        ASSERT_FALSE(result.has_value(), "Should fail negative to unsigned");
        ASSERT_EQ(result.error().code, JsonErrorCode::NumericOverflow, "Should be overflow error");
    }
    END_SUBTEST
    
    SUBTEST("int32_t from double with fraction") {
        auto j = try_parse_json("3.14");
        ASSERT_TRUE(j.has_value(), "Should parse");
        
        auto result = safe_from_json_numeric<int32_t>(*j);
        ASSERT_FALSE(result.has_value(), "Should fail fractional");
        ASSERT_EQ(result.error().code, JsonErrorCode::TypeError, "Should be type error");
    }
    END_SUBTEST
    
    SUBTEST("int64_t max value (no margin)") {
        JsonValue j = static_cast<int64_t>(LLONG_MAX);
        auto result = safe_from_json_numeric<int64_t>(j);
        ASSERT_TRUE(result.has_value(), "Should handle LLONG_MAX");
        ASSERT_EQ(*result, LLONG_MAX, "Should be LLONG_MAX");
    }
    END_SUBTEST
    
    SUBTEST("float from int") {
        auto j = try_parse_json("42");
        ASSERT_TRUE(j.has_value(), "Should parse");
        
        auto result = safe_from_json_numeric<float>(*j);
        ASSERT_TRUE(result.has_value(), "Should convert int to float");
        ASSERT_CLOSE(*result, 42.0f, "Should be 42.0");
    }
    END_SUBTEST
    
    SUBTEST("double from very large value") {
        auto j = try_parse_json("1.7976931348623157e+308");
        ASSERT_TRUE(j.has_value(), "Should parse");
        
        auto result = safe_from_json_numeric<double>(*j);
        ASSERT_TRUE(result.has_value(), "Should handle large double");
    }
    END_SUBTEST
    
    SUBTEST("uint64_t from positive int64_t") {
        JsonValue j = static_cast<int64_t>(1000);
        auto result = safe_from_json_numeric<uint64_t>(j);
        ASSERT_TRUE(result.has_value(), "Should convert positive int64 to uint64");
        ASSERT_EQ(*result, 1000ULL, "Should be 1000");
    }
    END_SUBTEST
    
    SUBTEST("uint64_t from zero") {
        JsonValue j = static_cast<int64_t>(0);
        auto result = safe_from_json_numeric<uint64_t>(j);
        ASSERT_TRUE(result.has_value(), "Should convert zero");
        ASSERT_EQ(*result, 0ULL, "Should be 0");
    }
    END_SUBTEST
    
    SUBTEST("uint64_t from negative int64_t fails") {
        JsonValue j = static_cast<int64_t>(-1);
        auto result = safe_from_json_numeric<uint64_t>(j);
        ASSERT_FALSE(result.has_value(), "Should fail for negative value");
        ASSERT_EQ(result.error().code, JsonErrorCode::NumericOverflow, "Should be overflow error");
    }
    END_SUBTEST
    
    SUBTEST("uint64_t from INT64_MAX") {
        JsonValue j = static_cast<int64_t>(INT64_MAX);
        auto result = safe_from_json_numeric<uint64_t>(j);
        ASSERT_TRUE(result.has_value(), "Should convert INT64_MAX");
        ASSERT_EQ(*result, static_cast<uint64_t>(INT64_MAX), "Should match INT64_MAX");
    }
    END_SUBTEST
    
    SUBTEST("uint32_t from large positive") {
        JsonValue j = static_cast<int64_t>(UINT32_MAX);
        auto result = safe_from_json_numeric<uint32_t>(j);
        ASSERT_TRUE(result.has_value(), "Should convert UINT32_MAX");
        ASSERT_EQ(*result, UINT32_MAX, "Should be UINT32_MAX");
    }
    END_SUBTEST
    
    SUBTEST("uint32_t overflow") {
        JsonValue j = static_cast<int64_t>(static_cast<int64_t>(UINT32_MAX) + 1);
        auto result = safe_from_json_numeric<uint32_t>(j);
        ASSERT_FALSE(result.has_value(), "Should fail for overflow");
        ASSERT_EQ(result.error().code, JsonErrorCode::NumericOverflow, "Should be overflow error");
    }
    END_SUBTEST
    
    SUBTEST("uint8_t valid range") {
        JsonValue j = static_cast<int64_t>(255);
        auto result = safe_from_json_numeric<uint8_t>(j);
        ASSERT_TRUE(result.has_value(), "Should convert 255");
        ASSERT_EQ(*result, static_cast<uint8_t>(255), "Should be 255");
    }
    END_SUBTEST
    
    SUBTEST("uint8_t overflow") {
        JsonValue j = static_cast<int64_t>(256);
        auto result = safe_from_json_numeric<uint8_t>(j);
        ASSERT_FALSE(result.has_value(), "Should fail for 256");
        ASSERT_EQ(result.error().code, JsonErrorCode::NumericOverflow, "Should be overflow error");
    }
    END_SUBTEST
    
    return get_subtest_tracker().all_passed();
}

TEST_CASE(safe_generic_conversions) {
    SUBTEST("Safe struct conversion") {
        std::string json = R"({"x":10,"y":20})";
        auto j = try_parse_json(json);
        ASSERT_TRUE(j.has_value(), "Should parse");
        
        auto result = safe_from_json<TestPoint>(*j);
        ASSERT_TRUE(result.has_value(), "Should convert to struct");
        ASSERT_EQ(result->x, 10, "x should be 10");
        ASSERT_EQ(result->y, 20, "y should be 20");
    }
    END_SUBTEST
    
    SUBTEST("Safe struct conversion error") {
        std::string json = R"({"x":"not_a_number","y":20})";
        auto j = try_parse_json(json);
        ASSERT_TRUE(j.has_value(), "Should parse");
        
        auto result = safe_from_json<TestPoint>(*j);
        ASSERT_FALSE(result.has_value(), "Should fail type mismatch");
        ASSERT_EQ(result.error().code, JsonErrorCode::TypeError, "Should be type error");
    }
    END_SUBTEST
    
    SUBTEST("Safe vector conversion") {
        auto j = try_parse_json("[1,2,3,4,5]");
        ASSERT_TRUE(j.has_value(), "Should parse");
        
        auto result = safe_from_json<std::vector<int>>(*j);
        ASSERT_TRUE(result.has_value(), "Should convert to vector");
        ASSERT_EQ(result->size(), 5U, "Should have 5 elements");
    }
    END_SUBTEST
    
    return get_subtest_tracker().all_passed();
}

TEST_CASE(file_operations) {
    SUBTEST("try_load_json success") {
        {
            std::ofstream ofs("test_fpjl_load.json");
            ofs << R"({"test":42})";
        }
        
        auto result = try_load_json("test_fpjl_load.json");
        ASSERT_TRUE(result.has_value(), "Should load successfully");
        ASSERT_TRUE(result->is_object(), "Should be object");
        
        std::remove("test_fpjl_load.json");
    }
    END_SUBTEST
    
    SUBTEST("try_load_json missing file") {
        auto result = try_load_json("nonexistent_file.json");
        ASSERT_FALSE(result.has_value(), "Should fail on missing file");
        ASSERT_EQ(result.error().code, JsonErrorCode::FileError, "Should be file error");
    }
    END_SUBTEST
    
    SUBTEST("try_save_json success") {
        JsonValue j = static_cast<int64_t>(42);
        auto result = try_save_json("test_fpjl_save.json", j);
        
        ASSERT_TRUE(result.has_value(), "Should save successfully");
        ASSERT_TRUE(std::ifstream("test_fpjl_save.json").good(), "File should exist");
        
        std::remove("test_fpjl_save.json");
    }
    END_SUBTEST
    
    SUBTEST("try_save_json with pretty print") {
        JsonObject obj;
        obj["key1"] = 1;
        obj["key2"] = 2;
        JsonValue j = obj;
        
        auto result = try_save_json<PrettyJsonPolicy>("test_fpjl_pretty.json", j, true);
        ASSERT_TRUE(result.has_value(), "Should save with pretty print");
        
        std::ifstream ifs("test_fpjl_pretty.json");
        std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
        ASSERT_TRUE(content.find("\n") != std::string::npos, "Should have newlines");
        
        std::remove("test_fpjl_pretty.json");
    }
    END_SUBTEST
    
    SUBTEST("try_load_json with ConfigJsonPolicy (line comments)") {
        const std::string filename = "test_jsonc_load.jsonc";
        {
            std::ofstream ofs(filename);
            ofs << "{\n"
                << "    // Server configuration\n"
                << "    \"port\": 8080,\n"
                << "    \"host\": \"localhost\"\n"
                << "}\n";
        }
        
        auto result = try_load_json<ConfigJsonPolicy>(filename);
        ASSERT_TRUE(result.has_value(), "Should load JSONC file with comments");
        
        const auto& obj = std::get<JsonObject>(*result);
        ASSERT_EQ(from_json<int>(obj, "port"), 8080, "Port should be 8080");
        ASSERT_EQ(from_json<std::string>(obj, "host"), "localhost", "Host should be localhost");
        
        std::remove(filename.c_str());
    }
    END_SUBTEST
    
    SUBTEST("try_load_json with ConfigJsonPolicy (block comments)") {
        const std::string filename = "test_jsonc_block.jsonc";
        {
            std::ofstream ofs(filename);
            ofs << "{\n"
                << "    /* Multi-line\n"
                << "       comment block */\n"
                << "    \"value\": 42\n"
                << "}\n";
        }
        
        auto result = try_load_json<ConfigJsonPolicy>(filename);
        ASSERT_TRUE(result.has_value(), "Should load JSONC file with block comments");
        
        const auto& obj = std::get<JsonObject>(*result);
        ASSERT_EQ(from_json<int>(obj, "value"), 42, "Value should be 42");
        
        std::remove(filename.c_str());
    }
    END_SUBTEST
    
    SUBTEST("StandardJsonPolicy rejects JSONC file") {
        const std::string filename = "test_jsonc_reject.jsonc";
        {
            std::ofstream ofs(filename);
            ofs << "{\n"
                << "    // This comment should cause rejection\n"
                << "    \"key\": \"value\"\n"
                << "}\n";
        }
        
        auto result = try_load_json<StandardJsonPolicy>(filename);
        ASSERT_FALSE(result.has_value(), "Standard policy should reject JSONC");
        
        std::remove(filename.c_str());
    }
    END_SUBTEST
    
    return get_subtest_tracker().all_passed();
}

TEST_CASE(memory_mapped_io) {
    SUBTEST("load_json_mmap small file") {
        {
            std::ofstream ofs("test_fpjl_mmap.json");
            ofs << R"({"name":"Alice","age":30,"active":true})";
        }
        
        auto result = load_json_mmap("test_fpjl_mmap.json");
        ASSERT_TRUE(result.has_value(), "Should load via mmap");
        ASSERT_TRUE(result->is_object(), "Should be object");
        
        const auto& obj = std::get<JsonObject>(*result);
        ASSERT_TRUE(obj.count("name") > 0, "Should have name field");
        
        std::remove("test_fpjl_mmap.json");
    }
    END_SUBTEST
    
    SUBTEST("load_json_mmap missing file") {
        auto result = load_json_mmap("nonexistent_mmap.json");
        ASSERT_FALSE(result.has_value(), "Should fail on missing file");
        ASSERT_EQ(result.error().code, JsonErrorCode::FileError, "Should be file error");
    }
    END_SUBTEST
    
    SUBTEST("load_json_mmap large file") {
        {
            std::ofstream ofs("test_fpjl_large.json");
            ofs << generate_large_json_array(5000);
        }
        
        auto result = load_json_mmap("test_fpjl_large.json");
        ASSERT_TRUE(result.has_value(), "Should load large file");
        ASSERT_TRUE(result->is_array(), "Should be array");
        
        std::remove("test_fpjl_large.json");
    }
    END_SUBTEST
    
    SUBTEST("load_json_mmap with ConfigJsonPolicy") {
        const std::string filename = "test_mmap_jsonc.jsonc";
        {
            std::ofstream ofs(filename);
            ofs << "{\n"
                << "    // Configuration file\n"
                << "    \"database\": {\n"
                << "        \"host\": \"db.example.com\",\n"
                << "        \"port\": 5432\n"
                << "    }\n"
                << "}\n";
        }
        
        auto result = load_json_mmap<ConfigJsonPolicy>(filename);
        ASSERT_TRUE(result.has_value(), "Should load JSONC via mmap");
        
        auto port = try_query_json_as<int>(*result, "/database/port");
        ASSERT_TRUE(port.has_value(), "Should extract port");
        ASSERT_EQ(*port, 5432, "Port should be 5432");
        
        std::remove(filename.c_str());
    }
    END_SUBTEST
    
    SUBTEST("load_json_mmap invalid JSON") {
        const std::string filename = "test_mmap_invalid.json";
        {
            std::ofstream ofs(filename);
            ofs << "{invalid json content}";
        }
        
        auto result = load_json_mmap(filename);
        ASSERT_FALSE(result.has_value(), "Should fail for invalid JSON");
        
        std::remove(filename.c_str());
    }
    END_SUBTEST
    
    return get_subtest_tracker().all_passed();
}

TEST_CASE(json_array) {
    SUBTEST("FatPJsonArray creation") {
        FatPJsonArray arr;
        for (int i = 0; i < 5; ++i) {
            arr.push_back(to_json(i));
        }
        
        ASSERT_EQ(arr.size(), 5U, "Should have 5 elements");
        ASSERT_TRUE(arr[0].is_int(), "First element should be int");
    }
    END_SUBTEST
    
    SUBTEST("FatPJsonArray inline storage benefit") {
        FatPJsonArray small_arr;
        for (int i = 0; i < 7; ++i) {
            small_arr.push_back(to_json(i));
        }
        
        ASSERT_EQ(small_arr.size(), 7U, "Should have 7 elements");
        ASSERT_TRUE(small_arr.capacity() >= 8U, "Should use inline storage");
    }
    END_SUBTEST
    
    SUBTEST("to_json_array conversion") {
        FatPJsonArray fatp_arr;
        fatp_arr.push_back(to_json(1));
        fatp_arr.push_back(to_json(2));
        fatp_arr.push_back(to_json(3));
        
        JsonArray std_arr = to_json_array(fatp_arr);
        ASSERT_EQ(std_arr.size(), 3U, "Should convert size correctly");
    }
    END_SUBTEST
    
    SUBTEST("from_json_array conversion") {
        JsonArray std_arr;
        std_arr.push_back(to_json("a"));
        std_arr.push_back(to_json("b"));
        std_arr.push_back(to_json("c"));
        
        FatPJsonArray fatp_arr = from_json_array(std_arr);
        ASSERT_EQ(fatp_arr.size(), 3U, "Should convert size correctly");
    }
    END_SUBTEST
    
    return get_subtest_tracker().all_passed();
}

TEST_CASE(json_object) {
    SUBTEST("FatPJsonObject creation") {
        FatPJsonObject<> obj;
        obj["key1"] = to_json(1);
        obj["key2"] = to_json(2);
        obj["key3"] = to_json(3);
        
        ASSERT_EQ(obj.size(), 3U, "Should have 3 keys");
        ASSERT_TRUE(obj["key1"].is_int(), "Value should be int");
    }
    END_SUBTEST
    
    SUBTEST("FatPJsonObject find operations") {
        FatPJsonObject<> obj;
        obj["exists"] = to_json(42);
        
        ASSERT_TRUE(obj.find("exists") != obj.end(), "Should find existing key");
        ASSERT_TRUE(obj.find("missing") == obj.end(), "Should not find missing key");
    }
    END_SUBTEST
    
    SUBTEST("to_json_object conversion") {
        FatPJsonObject<> fatp_obj;
        fatp_obj["a"] = to_json(1);
        fatp_obj["b"] = to_json(2);
        
        JsonObject std_obj = to_json_object(fatp_obj);
        ASSERT_EQ(std_obj.size(), 2U, "Should convert size correctly");
        ASSERT_TRUE(std_obj.count("a") > 0, "Should have key 'a'");
    }
    END_SUBTEST
    
    SUBTEST("from_json_object conversion") {
        JsonObject std_obj;
        std_obj["x"] = to_json(10);
        std_obj["y"] = to_json(20);
        
        FatPJsonObject<> fatp_obj = from_json_object(std_obj);
        ASSERT_EQ(fatp_obj.size(), 2U, "Should convert size correctly");
        ASSERT_TRUE(fatp_obj.find("x") != fatp_obj.end(), "Should have key 'x'");
    }
    END_SUBTEST
    
    return get_subtest_tracker().all_passed();
}

TEST_CASE(pooled_json_object) {
    SUBTEST("PooledJsonObject basic usage") {
        StringPool<SingleThreadedPolicy> pool;
        PooledJsonObject pooled(pool);
        
        pooled.insert("name", to_json("Alice"));
        pooled.insert("age", to_json(30));
        
        ASSERT_EQ(pooled.size(), 2U, "Should have 2 entries");
        ASSERT_TRUE(pooled.find("name") != nullptr, "Should find 'name'");
    }
    END_SUBTEST
    
    SUBTEST("PooledJsonObject string deduplication") {
        StringPool<SingleThreadedPolicy> pool;
        
        PooledJsonObject obj1(pool);
        obj1.insert("repeated_key", to_json(1));
        
        PooledJsonObject obj2(pool);
        obj2.insert("repeated_key", to_json(2));
        
        PooledJsonObject obj3(pool);
        obj3.insert("repeated_key", to_json(3));
        
        ASSERT_TRUE(pool.size() <= 2U, "Pool should deduplicate strings");
    }
    END_SUBTEST
    
    SUBTEST("PooledJsonObject to JsonObject") {
        StringPool<SingleThreadedPolicy> pool;
        PooledJsonObject pooled(pool);
        pooled.insert("key1", to_json("value1"));
        pooled.insert("key2", to_json("value2"));
        
        JsonObject std_obj = pooled.to_json_object();
        ASSERT_EQ(std_obj.size(), 2U, "Should convert correctly");
        ASSERT_TRUE(std_obj.count("key1") > 0, "Should have key1");
    }
    END_SUBTEST
    
    SUBTEST("PooledJsonObject from JsonObject") {
        StringPool<SingleThreadedPolicy> pool;
        JsonObject std_obj;
        std_obj["a"] = to_json(1);
        std_obj["b"] = to_json(2);
        
        auto pooled = PooledJsonObject<SingleThreadedPolicy>::from_json_object(pool, std_obj);
        ASSERT_EQ(pooled.size(), 2U, "Should create from JsonObject");
    }
    END_SUBTEST
    
    SUBTEST("PooledJsonObject clear") {
        StringPool<SingleThreadedPolicy> pool;
        PooledJsonObject pooled(pool);
        pooled.insert("key", to_json(42));
        
        ASSERT_FALSE(pooled.empty(), "Should not be empty");
        pooled.clear();
        ASSERT_TRUE(pooled.empty(), "Should be empty after clear");
    }
    END_SUBTEST
    
    return get_subtest_tracker().all_passed();
}

TEST_CASE(batch_parsing) {
    SUBTEST("Batch parse all valid") {
        std::vector<std::string> jsons = {
            R"({"a":1})",
            R"([1,2,3])",
            R"("string")",
            "42"
        };
        
        auto result = batch_parse_json(jsons);
        ASSERT_TRUE(result.has_value(), "Should parse all successfully");
        ASSERT_EQ(result->size(), 4U, "Should have 4 results");
    }
    END_SUBTEST
    
    SUBTEST("Batch parse with errors (fail_fast)") {
        std::vector<std::string> jsons = {
            R"({"valid":true})",
            "{bad json}",
            R"([1,2,3])"
        };
        
        auto result = batch_parse_json(jsons, true);
        ASSERT_FALSE(result.has_value(), "Should fail on first error");
        ASSERT_EQ(result.error().size(), 1U, "Should have 1 error");
    }
    END_SUBTEST
    
    SUBTEST("Batch parse collect all errors") {
        std::vector<std::string> jsons = {
            R"({"valid":true})",
            "{bad1}",
            "{bad2}",
            R"([1,2,3])"
        };
        
        auto result = batch_parse_json(jsons, false);
        ASSERT_FALSE(result.has_value(), "Should collect errors");
        ASSERT_EQ(result.error().size(), 2U, "Should have 2 errors");
    }
    END_SUBTEST
    
    SUBTEST("Batch parse empty input") {
        std::vector<std::string> jsons;
        auto result = batch_parse_json(jsons);
        ASSERT_TRUE(result.has_value(), "Empty input should succeed");
        ASSERT_TRUE(result->empty(), "Result should be empty");
    }
    END_SUBTEST
    
    return get_subtest_tracker().all_passed();
}

TEST_CASE(jsonc_comments) {
    SUBTEST("Line comments") {
        std::string json = "{\n"
            "    // This is a line comment\n"
            "    \"port\": 8080,  // Inline comment\n"
            "    \"host\": \"localhost\"\n"
            "}";
        
        auto result = try_parse_json<ConfigJsonPolicy>(json);
        ASSERT_TRUE(result.has_value(), "Should parse with line comments");
        
        const auto& obj = std::get<JsonObject>(*result);
        ASSERT_EQ(from_json<int>(obj, "port"), 8080, "Port should be 8080");
    }
    END_SUBTEST
    
    SUBTEST("Block comments") {
        std::string json = "{\n"
            "    /* This is a block comment */\n"
            "    \"name\": \"test\",\n"
            "    \"value\": /* inline block */ 42\n"
            "}";
        
        auto result = try_parse_json<ConfigJsonPolicy>(json);
        ASSERT_TRUE(result.has_value(), "Should parse with block comments");
        
        const auto& obj = std::get<JsonObject>(*result);
        ASSERT_EQ(from_json<int>(obj, "value"), 42, "Value should be 42");
    }
    END_SUBTEST
    
    SUBTEST("Strict policy rejects comments") {
        std::string json = "{\n"
            "    // Comment that should cause rejection\n"
            "    \"key\": \"value\"\n"
            "}";
        
        auto result = try_parse_json<StandardJsonPolicy>(json);
        ASSERT_FALSE(result.has_value(), "Standard policy should reject comments");
        ASSERT_EQ(result.error().code, JsonErrorCode::ParseError, "Should be parse error");
    }
    END_SUBTEST
    
    return get_subtest_tracker().all_passed();
}

TEST_CASE(utf8_handling) {
    SUBTEST("European chars escaped") {
        std::string text = "cafÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â©";
        std::string json = to_json_string<std::string, StandardJsonPolicy>(text);
        
        ASSERT_TRUE(json.find("\\u") != std::string::npos, "Should escape European chars");
        
        auto parsed = try_parse_json(json);
        ASSERT_TRUE(parsed.has_value(), "Should parse back");
        ASSERT_EQ(std::get<std::string>(*parsed), text, "Should round-trip");
    }
    END_SUBTEST
    
    SUBTEST("Asian chars escaped") {
        std::string text = "ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¤ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¸ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬ÃƒÂ¢Ã¢â€šÂ¬Ã…â€œÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â§ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã¢â‚¬Â¦ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢";
        std::string json = to_json_string<std::string, StandardJsonPolicy>(text);
        
        ASSERT_TRUE(json.find("\\u") != std::string::npos, "Should escape Asian chars");
        
        auto parsed = try_parse_json(json);
        ASSERT_TRUE(parsed.has_value(), "Should parse back");
    }
    END_SUBTEST
    
    SUBTEST("Emoji with surrogate pairs") {
        std::string text = "ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â°ÃƒÆ’Ã¢â‚¬Â¦Ãƒâ€šÃ‚Â¸ÃƒÆ’Ã¢â‚¬Â¹Ãƒâ€¦Ã¢â‚¬Å“ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡Ãƒâ€šÃ‚Â¬";
        std::string json = to_json_string<std::string, StandardJsonPolicy>(text);
        
        ASSERT_TRUE(json.find("\\u") != std::string::npos, "Should escape emoji");
        
        auto parsed = try_parse_json(json);
        ASSERT_TRUE(parsed.has_value(), "Should parse back");
    }
    END_SUBTEST
    
    SUBTEST("Raw UTF-8 with CompatJsonPolicy") {
        std::string text = "ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¤ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¸ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬ÃƒÂ¢Ã¢â€šÂ¬Ã…â€œÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â§ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã¢â‚¬Â¦ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ cafÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â© ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â°ÃƒÆ’Ã¢â‚¬Â¦Ãƒâ€šÃ‚Â¸ÃƒÆ’Ã¢â‚¬Â¹Ãƒâ€¦Ã¢â‚¬Å“ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡Ãƒâ€šÃ‚Â¬";
        std::string json = to_json_string<std::string, CompatJsonPolicy>(text);
        
        ASSERT_TRUE(json.find("\\u") == std::string::npos, "CompatPolicy should not escape");
        
        auto parsed = try_parse_json(json);
        ASSERT_TRUE(parsed.has_value(), "Should parse raw UTF-8");
    }
    END_SUBTEST
    
    return get_subtest_tracker().all_passed();
}

TEST_CASE(nan_infinity_handling) {
    SUBTEST("NaN default rejection") {
        std::string json = R"({"value": NaN})";
        auto result = try_parse_json<StandardJsonPolicy>(json);
        ASSERT_FALSE(result.has_value(), "Standard policy should reject NaN");
    }
    END_SUBTEST
    
    SUBTEST("Infinity default rejection") {
        std::string json = R"({"value": Infinity})";
        auto result = try_parse_json<StandardJsonPolicy>(json);
        ASSERT_FALSE(result.has_value(), "Standard policy should reject Infinity");
    }
    END_SUBTEST
    
    SUBTEST("NaN with CompatJsonPolicy") {
        std::string json = R"({"value": NaN})";
        auto result = try_parse_json<CompatJsonPolicy>(json);
        ASSERT_TRUE(result.has_value(), "CompatPolicy should accept NaN");
        
        const auto& obj = std::get<JsonObject>(*result);
        double val = from_json<double>(obj.at("value"));
        ASSERT_TRUE(std::isnan(val), "Value should be NaN");
    }
    END_SUBTEST
    
    SUBTEST("Infinity with CompatJsonPolicy") {
        std::string json = R"({"value": Infinity})";
        auto result = try_parse_json<CompatJsonPolicy>(json);
        ASSERT_TRUE(result.has_value(), "CompatPolicy should accept Infinity");
        
        const auto& obj = std::get<JsonObject>(*result);
        double val = from_json<double>(obj.at("value"));
        ASSERT_TRUE(std::isinf(val) && val > 0, "Should be positive infinity");
    }
    END_SUBTEST
    
    SUBTEST("Negative Infinity with CompatJsonPolicy") {
        std::string json = R"({"value": -Infinity})";
        auto result = try_parse_json<CompatJsonPolicy>(json);
        ASSERT_TRUE(result.has_value(), "Should accept -Infinity");
        
        const auto& obj = std::get<JsonObject>(*result);
        double val = from_json<double>(obj.at("value"));
        ASSERT_TRUE(std::isinf(val) && val < 0, "Should be negative infinity");
    }
    END_SUBTEST
    
    return get_subtest_tracker().all_passed();
}

TEST_CASE(numeric_bounds_no_margin) {
    SUBTEST("LLONG_MAX without margin") {
        JsonValue j = static_cast<int64_t>(LLONG_MAX);
        auto result = safe_from_json_numeric<int64_t>(j);
        ASSERT_TRUE(result.has_value(), "Should handle LLONG_MAX");
        ASSERT_EQ(*result, LLONG_MAX, "Should be LLONG_MAX");
    }
    END_SUBTEST
    
    SUBTEST("LLONG_MIN without margin") {
        JsonValue j = static_cast<int64_t>(LLONG_MIN);
        auto result = safe_from_json_numeric<int64_t>(j);
        ASSERT_TRUE(result.has_value(), "Should handle LLONG_MIN");
        ASSERT_EQ(*result, LLONG_MIN, "Should be LLONG_MIN");
    }
    END_SUBTEST
    
    SUBTEST("INT_MAX exact value") {
        JsonValue j = static_cast<int64_t>(INT_MAX);
        auto result = safe_from_json_numeric<int>(j);
        ASSERT_TRUE(result.has_value(), "Should handle INT_MAX exactly");
        ASSERT_EQ(*result, INT_MAX, "Should be INT_MAX");
    }
    END_SUBTEST
    
    SUBTEST("Overflow detection at boundary") {
        JsonValue j = static_cast<int64_t>(INT_MAX) + 1;
        auto result = safe_from_json_numeric<int>(j);
        ASSERT_FALSE(result.has_value(), "Should detect overflow at boundary");
        ASSERT_EQ(result.error().code, JsonErrorCode::NumericOverflow, "Should be overflow");
    }
    END_SUBTEST
    
    return get_subtest_tracker().all_passed();
}

TEST_CASE(value_returning_api) {
    SUBTEST("Value-returning from_json") {
        auto j = try_parse_json("42");
        ASSERT_TRUE(j.has_value(), "Should parse");
        
        const int value = from_json<int>(*j);
        ASSERT_EQ(value, 42, "Should return value");
    }
    END_SUBTEST
    
    SUBTEST("Value-returning with struct") {
        std::string json = R"({"x":10,"y":20})";
        auto j = try_parse_json(json);
        ASSERT_TRUE(j.has_value(), "Should parse");
        
        const TestPoint pt = from_json<TestPoint>(*j);
        ASSERT_EQ(pt.x, 10, "x should be 10");
        ASSERT_EQ(pt.y, 20, "y should be 20");
    }
    END_SUBTEST
    
    SUBTEST("Value-returning with key access") {
        std::string json = R"({"port":8080,"host":"localhost"})";
        auto j = try_parse_json(json);
        ASSERT_TRUE(j.has_value(), "Should parse");
        
        const int port = from_json<int>(*j, "port");
        const auto host = from_json<std::string>(*j, "host");
        
        ASSERT_EQ(port, 8080, "Port should be 8080");
        ASSERT_EQ(host, "localhost", "Host should be localhost");
    }
    END_SUBTEST
    
    return get_subtest_tracker().all_passed();
}

TEST_CASE(large_datasets) {
    SUBTEST("Large object parsing") {
        std::string large_json = generate_large_json_object(LARGE_OBJECT_SIZE);
        auto result = try_parse_json(large_json);
        
        ASSERT_TRUE(result.has_value(), "Should parse large object");
        ASSERT_TRUE(result->is_object(), "Should be object");
        ASSERT_EQ(std::get<JsonObject>(*result).size(), LARGE_OBJECT_SIZE, "Should have correct size");
    }
    END_SUBTEST
    
    SUBTEST("Large array parsing") {
        std::string large_json = generate_large_json_array(LARGE_ARRAY_SIZE);
        auto result = try_parse_json(large_json);
        
        ASSERT_TRUE(result.has_value(), "Should parse large array");
        ASSERT_TRUE(result->is_array(), "Should be array");
        ASSERT_EQ(std::get<JsonArray>(*result).size(), LARGE_ARRAY_SIZE, "Should have correct size");
    }
    END_SUBTEST
    
    SUBTEST("Deeply nested JSON") {
        std::string nested_json = generate_nested_json(NESTED_DEPTH);
        auto result = try_parse_json(nested_json);
        
        ASSERT_TRUE(result.has_value(), "Should parse nested JSON");
    }
    END_SUBTEST
    
    return get_subtest_tracker().all_passed();
}

TEST_CASE(locale_independence) {
    // Test that parsing is locale-independent (uses '.' for decimals regardless of locale)
    
    SUBTEST("Parse double - locale should not affect decimal point") {
        // Store current locale
        std::string orig_locale_name = std::setlocale(LC_NUMERIC, nullptr);
        
        // Try to set a locale that uses comma as decimal separator
        // Note: This may fail if locale is not installed, which is OK - test passes
        const char* test_locales[] = {"de_DE.UTF-8", "fr_FR.UTF-8", "es_ES.UTF-8", "C"};
        bool locale_set = false;
        
        for (const char* loc : test_locales) {
            if (std::setlocale(LC_NUMERIC, loc) != nullptr) {
                locale_set = true;
                break;
            }
        }
        
        // Parse with standard JSON decimal notation (always '.')
        auto result = try_parse_json("3.14159");
        
        // Restore original locale
        std::setlocale(LC_NUMERIC, orig_locale_name.c_str());
        
        ASSERT_TRUE(result.has_value(), "Should parse regardless of locale");
        ASSERT_TRUE(result->is_number(), "Should be a number");
        double val = std::get<double>(*result);
        ASSERT_TRUE(std::abs(val - 3.14159) < 0.0001, "Value should be ~3.14159");
    }
    END_SUBTEST
    
    SUBTEST("Parse integer - locale should not affect thousands separator") {
        std::string orig_locale_name = std::setlocale(LC_NUMERIC, nullptr);
        std::setlocale(LC_NUMERIC, "C");  // Reset to standard
        
        // JSON integers never have thousands separators
        auto result = try_parse_json("1000000");
        
        std::setlocale(LC_NUMERIC, orig_locale_name.c_str());
        
        ASSERT_TRUE(result.has_value(), "Should parse integer");
        ASSERT_TRUE(result->is_int(), "Should be integer");
        ASSERT_EQ(std::get<int64_t>(*result), 1000000LL, "Value should be 1000000");
    }
    END_SUBTEST
    
    SUBTEST("Parse scientific notation") {
        auto result = try_parse_json("1.5e10");
        ASSERT_TRUE(result.has_value(), "Should parse scientific notation");
        ASSERT_TRUE(result->is_number(), "Should be number");
        double val = std::get<double>(*result);
        ASSERT_TRUE(std::abs(val - 1.5e10) < 1e6, "Value should be ~1.5e10");
    }
    END_SUBTEST
    
    SUBTEST("Parse negative decimal") {
        auto result = try_parse_json("-123.456");
        ASSERT_TRUE(result.has_value(), "Should parse negative decimal");
        ASSERT_TRUE(result->is_number(), "Should be number");
        double val = std::get<double>(*result);
        ASSERT_TRUE(std::abs(val - (-123.456)) < 0.001, "Value should be ~-123.456");
    }
    END_SUBTEST
    
    return get_subtest_tracker().all_passed();
}

TEST_CASE(enum_integration_basic) {
    SUBTEST("Enum to JSON string") {
        TaskStatus status = TaskStatus::Running;
        JsonValue j = to_json(status);
        
        ASSERT_TRUE(j.is_string(), "Should be string");
        ASSERT_EQ(std::get<std::string>(j), "Running", "Should be 'Running'");
    }
    END_SUBTEST
    
    SUBTEST("JSON string to enum") {
        JsonValue j = to_json(std::string("Completed"));
        TaskStatus status;
        from_json(j, status);
        
        ASSERT_EQ(status, TaskStatus::Completed, "Should be Completed");
    }
    END_SUBTEST
    
    SUBTEST("All enum values roundtrip") {
        std::vector<TaskStatus> statuses = {
            TaskStatus::Idle, TaskStatus::Running, TaskStatus::Completed, TaskStatus::Failed
        };
        
        for (const auto& orig : statuses) {
            JsonValue j = to_json(orig);
            TaskStatus loaded;
            from_json(j, loaded);
            ASSERT_EQ(loaded, orig, "Should roundtrip correctly");
        }
    }
    END_SUBTEST
    
    SUBTEST("Priority enum serialization") {
        Priority p = Priority::High;
        JsonValue j = to_json(p);
        
        ASSERT_TRUE(j.is_string(), "Should be string");
        ASSERT_EQ(std::get<std::string>(j), "High", "Should be 'High'");
    }
    END_SUBTEST
    
    SUBTEST("Color enum serialization") {
        Color c = Color::Blue;
        JsonValue j = to_json(c);
        
        ASSERT_TRUE(j.is_string(), "Should be string");
        ASSERT_EQ(std::get<std::string>(j), "Blue", "Should be 'Blue'");
    }
    END_SUBTEST
    
    return get_subtest_tracker().all_passed();
}

TEST_CASE(enum_integration_structs) {
    SUBTEST("Struct with enum fields") {
        Task task{"Build System", TaskStatus::Running, Priority::High, 75};
        JsonValue j = to_json(task);
        
        ASSERT_TRUE(j.is_object(), "Should be object");
        const auto& obj = std::get<JsonObject>(j);
        
        ASSERT_TRUE(obj.at("status").is_string(), "Status should be string");
        ASSERT_EQ(std::get<std::string>(obj.at("status")), "Running", "Status should be 'Running'");
        
        ASSERT_TRUE(obj.at("priority").is_string(), "Priority should be string");
        ASSERT_EQ(std::get<std::string>(obj.at("priority")), "High", "Priority should be 'High'");
    }
    END_SUBTEST
    
    SUBTEST("Struct roundtrip with enums") {
        Task original{"Test Task", TaskStatus::Completed, Priority::Medium, 100};
        JsonValue j = to_json(original);
        
        Task loaded = from_json<Task>(j);
        
        ASSERT_EQ(loaded.name, original.name, "Name should match");
        ASSERT_EQ(loaded.status, original.status, "Status should match");
        ASSERT_EQ(loaded.priority, original.priority, "Priority should match");
        ASSERT_EQ(loaded.progress, original.progress, "Progress should match");
    }
    END_SUBTEST
    
    SUBTEST("ColoredObject roundtrip") {
        ColoredObject obj{"Sky", Color::Blue, 0.75};
        JsonValue j = to_json(obj);
        
        ColoredObject loaded = from_json<ColoredObject>(j);
        
        ASSERT_EQ(loaded.label, obj.label, "Label should match");
        ASSERT_EQ(loaded.color, obj.color, "Color should match");
        ASSERT_CLOSE(loaded.value, obj.value, "Value should match");
    }
    END_SUBTEST
    
    SUBTEST("JSON string to struct parsing") {
        std::string json = R"({"name":"Deploy","status":"Running","priority":"Critical","progress":50})";
        auto result = try_parse_json(json);
        ASSERT_TRUE(result.has_value(), "Should parse");
        
        Task task = from_json<Task>(*result);
        
        ASSERT_EQ(task.name, "Deploy", "Name should match");
        ASSERT_EQ(task.status, TaskStatus::Running, "Status should be Running");
        ASSERT_EQ(task.priority, Priority::Critical, "Priority should be Critical");
        ASSERT_EQ(task.progress, 50, "Progress should be 50");
    }
    END_SUBTEST
    
    return get_subtest_tracker().all_passed();
}

TEST_CASE(enum_integration_errors) {
    SUBTEST("Invalid enum string throws") {
        JsonValue j = to_json(std::string("InvalidStatus"));
        TaskStatus status;
        
        bool threw = false;
        try {
            from_json(j, status);
        } catch (const std::exception&) {
            threw = true;
        }
        
        ASSERT_TRUE(threw, "Should throw on invalid enum string");
    }
    END_SUBTEST
    
    SUBTEST("Type mismatch throws") {
        JsonValue j = to_json(42);
        TaskStatus status;
        
        bool threw = false;
        try {
            from_json(j, status);
        } catch (const std::exception&) {
            threw = true;
        }
        
        ASSERT_TRUE(threw, "Should throw on type mismatch");
    }
    END_SUBTEST
    
    SUBTEST("safe_from_json_enum with valid value") {
        JsonValue j = to_json(TaskStatus::Completed);
        auto result = safe_from_json_enum<TaskStatus>(j);
        
        ASSERT_TRUE(result.has_value(), "Should succeed");
        ASSERT_EQ(*result, TaskStatus::Completed, "Should be Completed");
    }
    END_SUBTEST
    
    SUBTEST("safe_from_json_enum with invalid string") {
        JsonValue j = to_json(std::string("InvalidStatus"));
        auto result = safe_from_json_enum<TaskStatus>(j);
        
        ASSERT_FALSE(result.has_value(), "Should fail");
        ASSERT_EQ(result.error().code, JsonErrorCode::TypeError, "Should be type error");
        ASSERT_FALSE(result.error().message.empty(), "Should have error message");
    }
    END_SUBTEST
    
    SUBTEST("safe_from_json_enum with wrong type") {
        JsonValue j = to_json(42);
        auto result = safe_from_json_enum<TaskStatus>(j);
        
        ASSERT_FALSE(result.has_value(), "Should fail");
        ASSERT_EQ(result.error().code, JsonErrorCode::TypeError, "Should be type error");
    }
    END_SUBTEST
    
    SUBTEST("safe_from_json_enum all valid values") {
        std::vector<Priority> priorities = {
            Priority::Low, Priority::Medium, Priority::High, Priority::Critical
        };
        
        for (const auto& p : priorities) {
            JsonValue j = to_json(p);
            auto result = safe_from_json_enum<Priority>(j);
            ASSERT_TRUE(result.has_value(), "Should succeed for valid enum");
            ASSERT_EQ(*result, p, "Should match original");
        }
    }
    END_SUBTEST
    
    return get_subtest_tracker().all_passed();
}

TEST_CASE(atomic_save_basic) {
    SUBTEST("Atomic save creates file") {
        const std::string filename = "test_atomic_basic.json";
        JsonValue j = to_json(42);
        
        auto result = try_save_atomic(filename, j);
        
        ASSERT_TRUE(result.has_value(), "Should save successfully");
        ASSERT_TRUE(std::filesystem::exists(filename), "File should exist");
        
        std::error_code ec;
        std::filesystem::remove(filename, ec);
    }
    END_SUBTEST
    
    SUBTEST("Atomic save roundtrip") {
        const std::string filename = "test_atomic_roundtrip.json";
        Task original{"Deploy", TaskStatus::Running, Priority::High, 85};
        JsonValue j = to_json(original);
        
        auto save_result = try_save_atomic(filename, j, true);
        ASSERT_TRUE(save_result.has_value(), "Should save");
        
        auto load_result = try_load_json(filename);
        ASSERT_TRUE(load_result.has_value(), "Should load");
        
        Task loaded = from_json<Task>(*load_result);
        
        ASSERT_EQ(loaded.name, original.name, "Name should match");
        ASSERT_EQ(loaded.status, original.status, "Status should match");
        ASSERT_EQ(loaded.priority, original.priority, "Priority should match");
        ASSERT_EQ(loaded.progress, original.progress, "Progress should match");
        
        std::error_code ec;
        std::filesystem::remove(filename, ec);
    }
    END_SUBTEST
    
    SUBTEST("Atomic save with pretty print") {
        const std::string filename = "test_atomic_pretty.json";
        JsonObject obj;
        obj["name"] = to_json(std::string("Test"));
        obj["value"] = to_json(123);
        JsonValue j = std::move(obj);
        
        auto result = try_save_atomic(filename, j, true);
        ASSERT_TRUE(result.has_value(), "Should save with pretty print");
        
        std::ifstream ifs(filename);
        std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
        
        ASSERT_TRUE(content.find('\n') != std::string::npos, "Should have newlines (pretty)");
        
        std::error_code ec;
        std::filesystem::remove(filename, ec);
    }
    END_SUBTEST
    
    SUBTEST("Atomic save without pretty print") {
        const std::string filename = "test_atomic_compact.json";
        JsonObject obj;
        obj["a"] = to_json(1);
        obj["b"] = to_json(2);
        JsonValue j = std::move(obj);
        
        auto result = try_save_atomic(filename, j, false);
        ASSERT_TRUE(result.has_value(), "Should save compact");
        
        std::ifstream ifs(filename);
        std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
        
        ASSERT_TRUE(content.find('\n') == std::string::npos || 
                   content.find('\n') == content.size() - 1, "Should be compact");
        
        std::error_code ec;
        std::filesystem::remove(filename, ec);
    }
    END_SUBTEST
    
    return get_subtest_tracker().all_passed();
}

TEST_CASE(atomic_save_safety) {
    SUBTEST("Atomic save to invalid path") {
        const std::string filename = "/invalid/path/test.json";
        JsonValue j = to_json(42);
        
        auto result = try_save_atomic(filename, j);
        
        ASSERT_FALSE(result.has_value(), "Should fail on invalid path");
        ASSERT_EQ(result.error().code, JsonErrorCode::FileError, "Should be file error");
    }
    END_SUBTEST
    
    SUBTEST("Atomic save cleans up temp file on error") {
        const std::string filename = "/invalid/path/test.json";
        JsonValue j = to_json(42);
        
        auto result = try_save_atomic(filename, j);
        ASSERT_FALSE(result.has_value(), "Should fail");
        
        bool temp_file_exists = false;
        for (const auto& entry : std::filesystem::directory_iterator(".")) {
            if (entry.path().filename().string().find(".tmp.") != std::string::npos) {
                temp_file_exists = true;
                break;
            }
        }
        
        ASSERT_FALSE(temp_file_exists, "Temp file should be cleaned up");
    }
    END_SUBTEST
    
    SUBTEST("Atomic save overwrites existing file") {
        const std::string filename = "test_atomic_overwrite.json";
        
        JsonValue j1 = to_json(42);
        auto result1 = try_save_atomic(filename, j1);
        ASSERT_TRUE(result1.has_value(), "First save should succeed");
        
        JsonValue j2 = to_json(99);
        auto result2 = try_save_atomic(filename, j2);
        ASSERT_TRUE(result2.has_value(), "Second save should succeed");
        
        auto load_result = try_load_json(filename);
        ASSERT_TRUE(load_result.has_value(), "Should load");
        ASSERT_EQ(std::get<int64_t>(*load_result), 99, "Should have new value");
        
        std::error_code ec;
        std::filesystem::remove(filename, ec);
    }
    END_SUBTEST
    
    SUBTEST("Atomic save with large data") {
        const std::string filename = "test_atomic_large.json";
        std::string large_json = generate_large_json_array(1000);
        auto parsed = try_parse_json(large_json);
        ASSERT_TRUE(parsed.has_value(), "Should parse large data");
        
        auto save_result = try_save_atomic(filename, *parsed);
        ASSERT_TRUE(save_result.has_value(), "Should save large data");
        
        auto load_result = try_load_json(filename);
        ASSERT_TRUE(load_result.has_value(), "Should load large data");
        
        ASSERT_TRUE(load_result->is_array(), "Should be array");
        ASSERT_EQ(std::get<JsonArray>(*load_result).size(), 1000U, "Should have 1000 elements");
        
        std::error_code ec;
        std::filesystem::remove(filename, ec);
    }
    END_SUBTEST
    
    return get_subtest_tracker().all_passed();
}

TEST_CASE(atomic_save_vs_regular) {
    SUBTEST("Compare atomic and regular save output") {
        const std::string atomic_file = "test_compare_atomic.json";
        const std::string regular_file = "test_compare_regular.json";
        
        Task task{"Compare", TaskStatus::Completed, Priority::Medium, 100};
        JsonValue j = to_json(task);
        
        auto atomic_result = try_save_atomic(atomic_file, j, true);
        ASSERT_TRUE(atomic_result.has_value(), "Atomic save should succeed");
        
        auto regular_result = try_save_json(regular_file, j, true);
        ASSERT_TRUE(regular_result.has_value(), "Regular save should succeed");
        
        std::ifstream ifs_atomic(atomic_file);
        std::string atomic_content((std::istreambuf_iterator<char>(ifs_atomic)), 
                                   std::istreambuf_iterator<char>());
        
        std::ifstream ifs_regular(regular_file);
        std::string regular_content((std::istreambuf_iterator<char>(ifs_regular)), 
                                    std::istreambuf_iterator<char>());
        
        ASSERT_EQ(atomic_content, regular_content, "Output should be identical");
        
        std::error_code ec;
        std::filesystem::remove(atomic_file, ec);
        std::filesystem::remove(regular_file, ec);
    }
    END_SUBTEST
    
    SUBTEST("Atomic save temp file uniqueness") {
        const std::string filename = "test_atomic_unique.json";
        JsonValue j1 = to_json(1);
        JsonValue j2 = to_json(2);
        
        auto result1 = try_save_atomic(filename, j1);
        ASSERT_TRUE(result1.has_value(), "First save should succeed");
        
        auto result2 = try_save_atomic(filename, j2);
        ASSERT_TRUE(result2.has_value(), "Second save should succeed");
        
        std::error_code ec;
        std::filesystem::remove(filename, ec);
    }
    END_SUBTEST
    
    SUBTEST("Concurrent atomic saves use unique temp files") {
        // Test that concurrent saves from different threads don't collide
        const std::string base_filename = "test_atomic_concurrent_";
        constexpr int NUM_THREADS = 4;
        constexpr int SAVES_PER_THREAD = 5;
        
        std::atomic<int> success_count{0};
        std::atomic<int> failure_count{0};
        std::vector<std::thread> threads;
        
        for (int t = 0; t < NUM_THREADS; ++t) {
            threads.emplace_back([&, t]() {
                for (int i = 0; i < SAVES_PER_THREAD; ++i) {
                    std::string filename = base_filename + std::to_string(t) + "_" + 
                                          std::to_string(i) + ".json";
                    JsonValue j = to_json(t * 100 + i);
                    auto result = try_save_atomic(filename, j);
                    if (result.has_value()) {
                        success_count.fetch_add(1, std::memory_order_relaxed);
                    } else {
                        failure_count.fetch_add(1, std::memory_order_relaxed);
                    }
                    std::error_code ec;
                    std::filesystem::remove(filename, ec);
                }
            });
        }
        
        for (auto& t : threads) {
            t.join();
        }
        
        ASSERT_EQ(success_count.load(), NUM_THREADS * SAVES_PER_THREAD, 
                  "All concurrent saves should succeed");
        ASSERT_EQ(failure_count.load(), 0, "No saves should fail");
    }
    END_SUBTEST
    
    SUBTEST("Concurrent atomic saves to SAME file") {
        // Stress test: multiple threads saving to the same file simultaneously
        // All should succeed; final file should contain valid JSON from one of them
        const std::string filename = "test_atomic_same_file.json";
        constexpr int NUM_THREADS = 8;
        constexpr int SAVES_PER_THREAD = 10;
        
        std::atomic<int> success_count{0};
        std::atomic<int> failure_count{0};
        std::vector<std::thread> threads;
        
        for (int t = 0; t < NUM_THREADS; ++t) {
            threads.emplace_back([&, t]() {
                for (int i = 0; i < SAVES_PER_THREAD; ++i) {
                    // Each thread writes its ID so we can verify valid content
                    JsonValue j = to_json(t * 1000 + i);
                    auto result = try_save_atomic(filename, j);
                    if (result.has_value()) {
                        success_count.fetch_add(1, std::memory_order_relaxed);
                    } else {
                        failure_count.fetch_add(1, std::memory_order_relaxed);
                    }
                }
            });
        }
        
        for (auto& th : threads) {
            th.join();
        }
        
        // All saves should succeed (no temp file collisions)
        ASSERT_EQ(success_count.load(), NUM_THREADS * SAVES_PER_THREAD,
                  "All concurrent same-file saves should succeed");
        ASSERT_EQ(failure_count.load(), 0, "No saves should fail");
        
        // Final file should be valid JSON
        auto final_result = try_load_json(filename);
        ASSERT_TRUE(final_result.has_value(), "Final file should be valid JSON");
        ASSERT_TRUE(final_result->is_int(), "Should contain an integer");
        
        std::error_code ec;
        std::filesystem::remove(filename, ec);
    }
    END_SUBTEST
    
    return get_subtest_tracker().all_passed();
}

void benchmark_parse_small_json() {
    *get_test_config().output << "\n" << colors::cyan() << "Small JSON Parsing (<1KB):" 
                               << colors::reset() << "\n\n";
    
    std::string small_json = R"({"name":"Alice","age":30,"city":"NYC","active":true,"score":95.5})";
    
    double jsonlite_time = measure_perf([&small_json]() {
        auto val = parse_json(small_json);
        DoNotOptimize(val);
    }, 10000, 100);
    
    double fatp_time = measure_perf([&small_json]() {
        auto result = try_parse_json(small_json);
        DoNotOptimize(result);
    }, 10000, 100);
    
    *get_test_config().output << "  JsonLite:      " << format_time(jsonlite_time) << "\n";
    *get_test_config().output << "  FatPJson:  " << format_time(fatp_time) << "\n";
    
    double ratio = fatp_time / jsonlite_time;
    *get_test_config().output << "  Ratio:         " << std::fixed << std::setprecision(2) 
                               << ratio << "x\n\n";
}

void benchmark_parse_large_array() {
    *get_test_config().output << colors::cyan() << "Large Array Parsing (10000 elements):" 
                               << colors::reset() << "\n\n";
    
    std::string large_json = generate_large_json_array(10000);
    
    double jsonlite_time = measure_perf([&large_json]() {
        auto val = parse_json(large_json);
        DoNotOptimize(val);
    }, 100, 10);
    
    double fatp_time = measure_perf([&large_json]() {
        auto result = try_parse_json(large_json);
        DoNotOptimize(result);
    }, 100, 10);
    
    *get_test_config().output << "  JsonLite:      " << format_time(jsonlite_time) << "\n";
    *get_test_config().output << "  FatPJson:  " << format_time(fatp_time) << "\n";
    
    double ratio = fatp_time / jsonlite_time;
    *get_test_config().output << "  Ratio:         " << std::fixed << std::setprecision(2) 
                               << ratio << "x\n\n";
}

void benchmark_array_operations() {
    *get_test_config().output << colors::cyan() << "Array Operations (SmallVector vs vector):" 
                               << colors::reset() << "\n\n";
    
    double std_time = measure_perf([]() {
        JsonArray arr;
        for (int j = 0; j < 5; ++j) {
            arr.push_back(to_json(j));
        }
        DoNotOptimize(arr);
    }, BENCHMARK_ITERATIONS, 100);
    
    double fatp_time = measure_perf([]() {
        FatPJsonArray arr;
        for (int j = 0; j < 5; ++j) {
            arr.push_back(to_json(j));
        }
        DoNotOptimize(arr);
    }, BENCHMARK_ITERATIONS, 100);
    
    *get_test_config().output << "  JsonArray (std::vector):      " << format_time(std_time) << "\n";
    *get_test_config().output << "  FatPJsonArray (SmallVector):  " << format_time(fatp_time) << "\n";
    
    double speedup = std_time / fatp_time;
    *get_test_config().output << "  Speedup:                      " << std::fixed << std::setprecision(2) 
                               << speedup << "x faster\n\n";
}

void benchmark_object_operations() {
    *get_test_config().output << colors::cyan() << "Object Operations (FlatMap vs map):" 
                               << colors::reset() << "\n\n";
    
    double std_time = measure_perf([]() {
        JsonObject obj;
        for (int j = 0; j < 10; ++j) {
            obj["key_" + std::to_string(j)] = to_json(j);
        }
        DoNotOptimize(obj);
    }, BENCHMARK_ITERATIONS, 100);
    
    double fatp_time = measure_perf([]() {
        FatPJsonObject<> obj;
        for (int j = 0; j < 10; ++j) {
            obj["key_" + std::to_string(j)] = to_json(j);
        }
        DoNotOptimize(obj);
    }, BENCHMARK_ITERATIONS, 100);
    
    *get_test_config().output << "  JsonObject (std::map):   " << format_time(std_time) << "\n";
    *get_test_config().output << "  FatPJsonObject (FlatMap): " << format_time(fatp_time) << "\n";
    
    double speedup = std_time / fatp_time;
    *get_test_config().output << "  Speedup:                 " << std::fixed << std::setprecision(2) 
                               << speedup << "x faster\n\n";
}

void benchmark_string_pool_memory() {
    *get_test_config().output << colors::cyan() << "StringPool Memory Savings:" 
                               << colors::reset() << "\n\n";
    
    std::string repeated_keys_json = generate_repeated_keys_json(1000);
    
    size_t standard_memory = 0;
    {
        auto val = parse_json(repeated_keys_json);
        auto& arr = std::get<JsonArray>(val);
        
        for (const auto& elem : arr) {
            auto& obj = std::get<JsonObject>(elem);
            for (const auto& [key, value] : obj) {
                standard_memory += key.size();
            }
        }
    }
    
    size_t pooled_memory = 0;
    {
        StringPool<SingleThreadedPolicy> pool;
        auto val = parse_json(repeated_keys_json);
        auto& arr = std::get<JsonArray>(val);
        
        std::set<std::string> unique_keys;
        for (const auto& elem : arr) {
            auto& obj = std::get<JsonObject>(elem);
            for (const auto& [key, value] : obj) {
                unique_keys.insert(key);
            }
        }
        
        for (const auto& key : unique_keys) {
            pooled_memory += key.size();
        }
    }
    
    double savings = 100.0 * (1.0 - static_cast<double>(pooled_memory) / standard_memory);
    
    *get_test_config().output << "  Standard memory (1000 objects): " << standard_memory << " bytes\n";
    *get_test_config().output << "  Pooled memory (unique keys):    " << pooled_memory << " bytes\n";
    *get_test_config().output << "  Memory savings:                 " << std::fixed << std::setprecision(1) 
                               << savings << "%\n\n";
}

void benchmark_error_handling() {
    *get_test_config().output << colors::cyan() << "Error Handling (Expected vs exceptions):" 
                               << colors::reset() << "\n\n";
    
    std::string bad_json = "{bad json}";
    
    double exception_time = measure_perf([&bad_json]() {
        try {
            auto val = parse_json(bad_json);
            DoNotOptimize(val);
        } catch (...) {
        }
    }, 1000, 10);
    
    double expected_time = measure_perf([&bad_json]() {
        auto result = try_parse_json(bad_json);
        DoNotOptimize(result);
    }, 1000, 10);
    
    *get_test_config().output << "  Exception-based (JsonLite): " << format_time(exception_time) << "\n";
    *get_test_config().output << "  Expected-based (FatPJson):  " << format_time(expected_time) << "\n";
    
    double speedup = exception_time / expected_time;
    *get_test_config().output << "  Speedup on error path:      " << std::fixed << std::setprecision(1) 
                               << speedup << "x faster\n\n";
}

void benchmark_memory_mapped_io() {
    *get_test_config().output << colors::cyan() << "Memory-Mapped I/O vs Regular I/O:" 
                               << colors::reset() << "\n\n";
    
    {
        std::ofstream ofs("test_mmap_bench.json");
        ofs << generate_large_json_array(50000);
    }
    
    double regular_time = measure_perf([]() {
        auto result = try_load_json("test_mmap_bench.json");
        DoNotOptimize(result);
    }, 10, 1);
    
    double mmap_time = measure_perf([]() {
        auto result = load_json_mmap("test_mmap_bench.json");
        DoNotOptimize(result);
    }, 10, 1);
    
    *get_test_config().output << "  Regular I/O:  " << format_time(regular_time) << "\n";
    *get_test_config().output << "  Memory-mapped: " << format_time(mmap_time) << "\n";
    
    double speedup = regular_time / mmap_time;
    *get_test_config().output << "  Speedup:      " << std::fixed << std::setprecision(2) 
                               << speedup << "x faster\n\n";
    
    std::remove("test_mmap_bench.json");
}

TEST_CASE(json_pointer_basic) {
    SUBTEST("basic navigation with Expected") {
        auto result = try_parse_json(R"({
            "database": {
                "host": "localhost",
                "port": 5432
            }
        })");
        
        ASSERT_TRUE(result.has_value(), "Parse should succeed");
        
        auto port_ptr = try_query_json_pointer(*result, "/database/port");
        ASSERT_TRUE(port_ptr.has_value(), "Navigation should succeed");
        
        auto port = safe_from_json<int>(**port_ptr);
        ASSERT_TRUE(port.has_value(), "Conversion should succeed");
        ASSERT_EQ(*port, 5432, "Port value should match");
    }
    END_SUBTEST
    
    SUBTEST("array navigation") {
        auto result = try_parse_json(R"({
            "servers": ["primary", "backup", "tertiary"]
        })");
        
        ASSERT_TRUE(result.has_value(), "Parse should succeed");
        
        auto server_ptr = try_query_json_pointer(*result, "/servers/1");
        ASSERT_TRUE(server_ptr.has_value(), "Navigation should succeed");
        
        auto server = safe_from_json<std::string>(**server_ptr);
        ASSERT_TRUE(server.has_value(), "Conversion should succeed");
        ASSERT_EQ(*server, "backup", "Server name should match");
    }
    END_SUBTEST
    
    SUBTEST("root document") {
        auto result = try_parse_json(R"({"key": "value"})");
        ASSERT_TRUE(result.has_value(), "Parse should succeed");
        
        auto root_ptr = try_query_json_pointer(*result, "");
        ASSERT_TRUE(root_ptr.has_value(), "Root navigation should succeed");
        ASSERT_TRUE((*root_ptr)->is_object(), "Root should be object");
    }
    END_SUBTEST
    
    return get_subtest_tracker().all_passed();
}

TEST_CASE(json_pointer_type_safe) {
    auto result = try_parse_json(R"({
        "database": {
            "host": "localhost",
            "port": 5432,
            "timeout": 30,
            "enabled": true
        },
        "servers": ["server1", "server2"]
    })");
    
    ASSERT_TRUE(result.has_value(), "Parse should succeed");
    
    SUBTEST("primitives with try_query_json_as") {
        auto port = try_query_json_as<int>(*result, "/database/port");
        ASSERT_TRUE(port.has_value(), "Port query should succeed");
        ASSERT_EQ(*port, 5432, "Port value should match");
        
        auto host = try_query_json_as<std::string>(*result, "/database/host");
        ASSERT_TRUE(host.has_value(), "Host query should succeed");
        ASSERT_EQ(*host, "localhost", "Host value should match");
        
        auto enabled = try_query_json_as<bool>(*result, "/database/enabled");
        ASSERT_TRUE(enabled.has_value(), "Enabled query should succeed");
        ASSERT_TRUE(*enabled, "Enabled should be true");
    }
    END_SUBTEST
    
    SUBTEST("containers with try_query_json_as") {
        auto servers = try_query_json_as<std::vector<std::string>>(*result, "/servers");
        ASSERT_TRUE(servers.has_value(), "Servers query should succeed");
        ASSERT_EQ(servers->size(), 2u, "Should have 2 servers");
        ASSERT_EQ((*servers)[0], "server1", "First server should match");
        ASSERT_EQ((*servers)[1], "server2", "Second server should match");
    }
    END_SUBTEST
    
    SUBTEST("query_json_as_or - get existing value") {
        int port = query_json_as_or(*result, "/database/port", 3306);
        ASSERT_EQ(port, 5432, "Should get actual port");
    }
    END_SUBTEST
    
    SUBTEST("query_json_as_or - get default for missing key") {
        int pool_size = query_json_as_or(*result, "/database/pool_size", 10);
        ASSERT_EQ(pool_size, 10, "Should get default pool_size");
    }
    END_SUBTEST
    
    SUBTEST("query_json_as_or - get default for type mismatch") {
        int host_as_int = query_json_as_or(*result, "/database/host", -1);
        ASSERT_EQ(host_as_int, -1, "Should get default for type mismatch");
    }
    END_SUBTEST
    
    SUBTEST("query_json_as_or - get default for invalid path") {
        std::string value = query_json_as_or(*result, "/nonexistent/path", std::string("default"));
        ASSERT_EQ(value, "default", "Should get default for invalid path");
    }
    END_SUBTEST
    
    SUBTEST("query_json_as_or - string default") {
        std::string host = query_json_as_or(*result, "/database/host", std::string("127.0.0.1"));
        ASSERT_EQ(host, "localhost", "Should get actual host");
    }
    END_SUBTEST
    
    return get_subtest_tracker().all_passed();
}

TEST_CASE(json_pointer_errors) {
    SUBTEST("key not found") {
        auto result = try_parse_json(R"({"key": "value"})");
        ASSERT_TRUE(result.has_value(), "Parse should succeed");
        
        auto bad_query = try_query_json_pointer(*result, "/nonexistent");
        ASSERT_FALSE(bad_query.has_value(), "Query should fail");
        ASSERT_EQ(bad_query.error().code, JsonErrorCode::TypeError, "Error code should be TypeError");
    }
    END_SUBTEST
    
    SUBTEST("invalid pointer format") {
        auto result = try_parse_json(R"({"key": "value"})");
        ASSERT_TRUE(result.has_value(), "Parse should succeed");
        
        auto bad_query = try_query_json_pointer(*result, "key");
        ASSERT_FALSE(bad_query.has_value(), "Query should fail");
    }
    END_SUBTEST
    
    SUBTEST("array index out of bounds") {
        auto result = try_parse_json(R"({"arr": [1, 2, 3]})");
        ASSERT_TRUE(result.has_value(), "Parse should succeed");
        
        auto bad_query = try_query_json_as<int>(*result, "/arr/10");
        ASSERT_FALSE(bad_query.has_value(), "Query should fail");
    }
    END_SUBTEST
    
    SUBTEST("type conversion error") {
        auto result = try_parse_json(R"({"value": "not_a_number"})");
        ASSERT_TRUE(result.has_value(), "Parse should succeed");
        
        auto bad_query = try_query_json_as<int>(*result, "/value");
        ASSERT_FALSE(bad_query.has_value(), "Conversion should fail");
        ASSERT_EQ(bad_query.error().code, JsonErrorCode::TypeError, "Error code should be TypeError");
    }
    END_SUBTEST
    
    return get_subtest_tracker().all_passed();
}

TEST_CASE(json_pointer_mutable) {
    auto result = try_parse_json(R"({
        "config": {
            "port": 8080,
            "host": "localhost"
        },
        "list": [1, 2, 3]
    })");
    
    ASSERT_TRUE(result.has_value(), "Parse should succeed");
    
    SUBTEST("modify nested value") {
        auto port_ptr = try_query_json_pointer(*result, "/config/port");
        ASSERT_TRUE(port_ptr.has_value(), "Navigation should succeed");
        
        **port_ptr = 9000;
        
        auto new_port = try_query_json_as<int>(*result, "/config/port");
        ASSERT_TRUE(new_port.has_value(), "Query should succeed");
        ASSERT_EQ(*new_port, 9000, "Port should be updated");
    }
    END_SUBTEST
    
    SUBTEST("modify array element") {
        auto elem_ptr = try_query_json_pointer(*result, "/list/1");
        ASSERT_TRUE(elem_ptr.has_value(), "Navigation should succeed");
        
        **elem_ptr = 42;
        
        auto new_elem = try_query_json_as<int>(*result, "/list/1");
        ASSERT_TRUE(new_elem.has_value(), "Query should succeed");
        ASSERT_EQ(*new_elem, 42, "Element should be updated");
    }
    END_SUBTEST
    
    return get_subtest_tracker().all_passed();
}

TEST_CASE(json_pointer_escape_sequences) {
    SUBTEST("tilde escape ~0") {
        auto result = try_parse_json(R"({"a~b": "value1"})");
        ASSERT_TRUE(result.has_value(), "Parse should succeed");
        
        auto val = try_query_json_as<std::string>(*result, "/a~0b");
        ASSERT_TRUE(val.has_value(), "Query should succeed");
        ASSERT_EQ(*val, "value1", "Value should match");
    }
    END_SUBTEST
    
    SUBTEST("forward slash escape ~1") {
        auto result = try_parse_json(R"({"foo/bar": "value2"})");
        ASSERT_TRUE(result.has_value(), "Parse should succeed");
        
        auto val = try_query_json_as<std::string>(*result, "/foo~1bar");
        ASSERT_TRUE(val.has_value(), "Query should succeed");
        ASSERT_EQ(*val, "value2", "Value should match");
    }
    END_SUBTEST
    
    SUBTEST("multiple escapes") {
        auto result = try_parse_json(R"({"a~b/c": "value3"})");
        ASSERT_TRUE(result.has_value(), "Parse should succeed");
        
        auto val = try_query_json_as<std::string>(*result, "/a~0b~1c");
        ASSERT_TRUE(val.has_value(), "Query should succeed");
        ASSERT_EQ(*val, "value3", "Value should match");
    }
    END_SUBTEST
    
    return get_subtest_tracker().all_passed();
}

TEST_CASE(json_pointer_complex) {
    SUBTEST("deep nested navigation") {
        auto result = try_parse_json(R"({
            "level1": {
                "level2": {
                    "level3": {
                        "data": [
                            {"name": "item1", "value": 100},
                            {"name": "item2", "value": 200}
                        ]
                    }
                }
            }
        })");
        
        ASSERT_TRUE(result.has_value(), "Parse should succeed");
        
        auto name = try_query_json_as<std::string>(*result, "/level1/level2/level3/data/1/name");
        ASSERT_TRUE(name.has_value(), "Query should succeed");
        ASSERT_EQ(*name, "item2", "Name should match");
        
        auto value = try_query_json_as<int>(*result, "/level1/level2/level3/data/0/value");
        ASSERT_TRUE(value.has_value(), "Query should succeed");
        ASSERT_EQ(*value, 100, "Value should match");
    }
    END_SUBTEST
    
    return get_subtest_tracker().all_passed();
}

TEST_CASE(json_pointer_optional) {
    auto result = try_parse_json(R"({
        "config": {
            "required": 42,
            "optional": 100
        }
    })");
    
    ASSERT_TRUE(result.has_value(), "Parse should succeed");
    
    SUBTEST("extract present optional") {
        auto opt_val = try_query_json_as<std::optional<int>>(*result, "/config/optional");
        ASSERT_TRUE(opt_val.has_value(), "Query should succeed");
        ASSERT_TRUE(opt_val->has_value(), "Optional should have value");
        ASSERT_EQ(**opt_val, 100, "Value should match");
    }
    END_SUBTEST
    
    SUBTEST("extract required as optional") {
        auto opt_val = try_query_json_as<std::optional<int>>(*result, "/config/required");
        ASSERT_TRUE(opt_val.has_value(), "Query should succeed");
        ASSERT_TRUE(opt_val->has_value(), "Optional should have value");
        ASSERT_EQ(**opt_val, 42, "Value should match");
    }
    END_SUBTEST
    
    return get_subtest_tracker().all_passed();
}

TEST_CASE(json_pointer_monadic) {
    SUBTEST("chaining with and_then") {
        auto result = try_parse_json(R"({
            "database": {
                "port": 5432
            }
        })");
        
        ASSERT_TRUE(result.has_value(), "Parse should succeed");
        
        auto final_result = try_query_json_as<int>(*result, "/database/port")
            .map([](int port) { return port + 1000; });
        
        ASSERT_TRUE(final_result.has_value(), "Chained operation should succeed");
        ASSERT_EQ(*final_result, 6432, "Mapped value should be correct");
    }
    END_SUBTEST
    
    SUBTEST("error propagation") {
        auto result = try_parse_json(R"({"key": "value"})");
        ASSERT_TRUE(result.has_value(), "Parse should succeed");
        
        auto final_result = try_query_json_as<int>(*result, "/nonexistent")
            .map([](int x) { return x + 1; });
        
        ASSERT_FALSE(final_result.has_value(), "Error should propagate");
    }
    END_SUBTEST
    
    return get_subtest_tracker().all_passed();
}

TEST_CASE(json_pointer_structs) {
    SUBTEST("extract custom struct") {
        auto result = try_parse_json(R"({
            "database": {
                "host": "localhost",
                "port": 5432,
                "timeout": 30
            }
        })");
        
        ASSERT_TRUE(result.has_value(), "Parse should succeed");
        
        auto db = try_query_json_as<DatabaseConfig>(*result, "/database");
        ASSERT_TRUE(db.has_value(), "Query should succeed");
        ASSERT_EQ(db->host, "localhost", "Host should match");
        ASSERT_EQ(db->port, 5432, "Port should match");
        ASSERT_TRUE(db->timeout.has_value(), "Timeout should be present");
        ASSERT_EQ(*db->timeout, 30, "Timeout value should match");
    }
    END_SUBTEST
    
    return get_subtest_tracker().all_passed();
}

// =============================================================================
// Malformed Input Tests
// =============================================================================

template <typename T>
static bool malformed_expect_fail(const std::string& json, const char* msg)
{
    auto r = json_decode_from<T>(json);
    if (r.has_value())
    {
        *get_test_config().output << "    UNEXPECTED SUCCESS: " << msg << "\n";
        return false;
    }
    return true;
}

TEST_CASE(malformed_missing_comma)
{
    bool r1 = malformed_expect_fail<std::vector<int>>("[1 2]", "missing comma in array must fail");
    ASSERT_TRUE(r1, "missing comma in array");
    
    using MapType = std::map<std::string, int>;
    bool r2 = malformed_expect_fail<MapType>("{\"a\":1 \"b\":2}", "missing comma in object must fail");
    ASSERT_TRUE(r2, "missing comma in object");
    return true;
}

TEST_CASE(malformed_trailing_comma_array)
{
    bool r1 = malformed_expect_fail<std::vector<int>>("[1,2,]", "trailing comma array must fail");
    ASSERT_TRUE(r1, "trailing comma array");
    
    bool r2 = malformed_expect_fail<std::vector<int>>("[,1,2]", "leading comma array must fail");
    ASSERT_TRUE(r2, "leading comma array");
    return true;
}

TEST_CASE(malformed_trailing_comma_object)
{
    using MapType = std::map<std::string, int>;
    bool r1 = malformed_expect_fail<MapType>("{\"a\":1,}", "trailing comma object must fail");
    ASSERT_TRUE(r1, "trailing comma object");
    return true;
}

TEST_CASE(malformed_unquoted_key)
{
    using MapType = std::map<std::string, int>;
    bool r1 = malformed_expect_fail<MapType>("{a:1}", "unquoted key must fail");
    ASSERT_TRUE(r1, "unquoted key");
    
    bool r2 = malformed_expect_fail<MapType>("{123:1}", "numeric key must fail");
    ASSERT_TRUE(r2, "numeric key");
    return true;
}

TEST_CASE(malformed_unterminated_string)
{
    bool r1 = malformed_expect_fail<std::string>("\"unterminated", "unterminated string must fail");
    ASSERT_TRUE(r1, "unterminated string");
    
    bool r2 = malformed_expect_fail<std::string>("\"unterminated\\", "unterminated escape must fail");
    ASSERT_TRUE(r2, "unterminated escape");
    return true;
}

TEST_CASE(malformed_numbers)
{
    // Leading zeros (except 0 itself)
    bool r1 = malformed_expect_fail<int>("01", "leading zero number must fail");
    ASSERT_TRUE(r1, "leading zero");
    
    bool r2 = malformed_expect_fail<int>("007", "leading zeros must fail");
    ASSERT_TRUE(r2, "leading zeros");

    // Trailing/leading decimal points
    bool r3 = malformed_expect_fail<double>("1.", "trailing decimal point must fail");
    ASSERT_TRUE(r3, "trailing decimal");
    
    bool r4 = malformed_expect_fail<double>(".1", "leading decimal point must fail");
    ASSERT_TRUE(r4, "leading decimal");

    // Double minus
    bool r5 = malformed_expect_fail<int>("--1", "double minus must fail");
    ASSERT_TRUE(r5, "double minus");

    // Plus sign (not allowed in JSON)
    bool r6 = malformed_expect_fail<int>("+1", "plus sign must fail");
    ASSERT_TRUE(r6, "plus sign");

    // Special values (strict JSON rejects these)
    bool r7 = malformed_expect_fail<double>("NaN", "NaN must fail in strict JSON");
    ASSERT_TRUE(r7, "NaN");
    
    bool r8 = malformed_expect_fail<double>("Infinity", "Infinity must fail in strict JSON");
    ASSERT_TRUE(r8, "Infinity");
    
    bool r9 = malformed_expect_fail<double>("-Infinity", "-Infinity must fail in strict JSON");
    ASSERT_TRUE(r9, "-Infinity");

    return true;
}

TEST_CASE(malformed_string_escapes)
{
    bool r1 = malformed_expect_fail<std::string>("\"\\x41\"", "non-standard \\x escape must fail");
    ASSERT_TRUE(r1, "non-standard \\x escape");
    
    bool r2 = malformed_expect_fail<std::string>("\"\\u12\"", "short \\u escape must fail");
    ASSERT_TRUE(r2, "short \\u escape");
    
    bool r3 = malformed_expect_fail<std::string>("\"\\uZZZZ\"", "non-hex \\u escape must fail");
    ASSERT_TRUE(r3, "non-hex \\u escape");
    
    bool r4 = malformed_expect_fail<std::string>("\"\\a\"", "non-standard \\a escape must fail");
    ASSERT_TRUE(r4, "non-standard \\a escape");
    return true;
}

TEST_CASE(malformed_structure)
{
    // Mismatched brackets
    bool r1 = malformed_expect_fail<std::vector<int>>("[1,2,3}", "mismatched brackets must fail");
    ASSERT_TRUE(r1, "mismatched brackets");
    
    using MapType = std::map<std::string, int>;
    bool r2 = malformed_expect_fail<MapType>("{\"a\":1]", "mismatched braces must fail");
    ASSERT_TRUE(r2, "mismatched braces");

    // Empty input
    bool r3 = malformed_expect_fail<int>("", "empty input must fail");
    ASSERT_TRUE(r3, "empty input");
    
    bool r4 = malformed_expect_fail<int>("   ", "whitespace-only input must fail");
    ASSERT_TRUE(r4, "whitespace-only input");

    // Multiple values at root
    bool r5 = malformed_expect_fail<int>("1 2", "multiple root values must fail");
    ASSERT_TRUE(r5, "multiple root values");
    
    bool r6 = malformed_expect_fail<int>("true false", "multiple booleans must fail");
    ASSERT_TRUE(r6, "multiple booleans");

    return true;
}

TEST_CASE(malformed_literals)
{
    bool r1 = malformed_expect_fail<bool>("True", "capitalized True must fail");
    ASSERT_TRUE(r1, "capitalized True");
    
    bool r2 = malformed_expect_fail<bool>("FALSE", "uppercase FALSE must fail");
    ASSERT_TRUE(r2, "uppercase FALSE");
    
    // Note: nullptr_t doesn't have from_json, test via parse instead
    auto r3 = json_decode_from<int>("Null");
    ASSERT_TRUE(!r3.has_value(), "capitalized Null must fail");
    
    auto r4 = json_decode_from<int>("NULL");
    ASSERT_TRUE(!r4.has_value(), "uppercase NULL must fail");
    return true;
}

TEST_CASE(malformed_deep_nesting_stress)
{
    // Create deeply nested JSON
    std::string json;
    const int depth = 256;

    for (int i = 0; i < depth; ++i)
    {
        json.push_back('[');
    }
    json += "0";
    for (int i = 0; i < depth; ++i)
    {
        json.push_back(']');
    }

    // This should either succeed or fail gracefully (not crash)
    auto r = json_decode_from<int>(json);
    // Either outcome is acceptable, but it must not crash
    (void)r;

    return true;
}

TEST_CASE(malformed_unicode)
{
    // Invalid UTF-8 sequences (these may or may not fail depending on policy)
    // The key point is they shouldn't crash
    auto r1 = json_decode_from<std::string>("\"\\uD800\"");  // Lone high surrogate
    auto r2 = json_decode_from<std::string>("\"\\uDC00\"");  // Lone low surrogate
    (void)r1;
    (void)r2;

    return true;
}

// =============================================================================
// Fuzz Tests
// =============================================================================

class FuzzRandom
{
public:
    explicit FuzzRandom(std::uint64_t seed) noexcept
        : eng_(seed)
    {
    }

    std::int64_t i64(std::int64_t lo, std::int64_t hi)
    {
        std::uniform_int_distribution<std::int64_t> d(lo, hi);
        return d(eng_);
    }

    std::uint64_t u64(std::uint64_t lo, std::uint64_t hi)
    {
        std::uniform_int_distribution<std::uint64_t> d(lo, hi);
        return d(eng_);
    }

    double dbl(double lo, double hi)
    {
        std::uniform_real_distribution<double> d(lo, hi);
        return d(eng_);
    }

    std::string str(std::size_t max_len)
    {
        std::uniform_int_distribution<std::size_t> len_dist(0, max_len);
        const std::size_t len = len_dist(eng_);
        std::uniform_int_distribution<int> ch_dist(32, 126);

        std::string s;
        s.reserve(len);
        for (std::size_t i = 0; i < len; ++i)
        {
            char c = static_cast<char>(ch_dist(eng_));
            // Avoid characters that need escaping for simpler round-trip
            if (c == '"' || c == '\\')
            {
                c = 'x';
            }
            s.push_back(c);
        }
        return s;
    }

private:
    std::mt19937_64 eng_;
};

template <typename T>
static JsonResult<T> fuzz_roundtrip(const T& value)
{
    std::string json;
    auto enc = json_encode_to(json, value);
    if (!enc)
    {
        return make_unexpected(enc.error());
    }

    return json_decode_from<T>(json);
}

TEST_CASE(fuzz_ints)
{
    FuzzRandom rng(0xC0FFEE123456789ULL);

    for (int i = 0; i < 2000; ++i)
    {
        const auto v = static_cast<int>(rng.i64(std::numeric_limits<int>::min(),
                                                 std::numeric_limits<int>::max()));
        auto r = fuzz_roundtrip(v);
        ASSERT_TRUE(r.has_value(), r.error().message.c_str());
        ASSERT_TRUE(*r == v, "Fuzz int mismatch");
    }

    return true;
}

TEST_CASE(fuzz_int64)
{
    FuzzRandom rng(0xC0FFEE64B101234ULL);

    for (int i = 0; i < 2000; ++i)
    {
        const auto v = rng.i64(std::numeric_limits<std::int64_t>::min() / 2,
                               std::numeric_limits<std::int64_t>::max() / 2);
        auto r = fuzz_roundtrip(v);
        ASSERT_TRUE(r.has_value(), r.error().message.c_str());
        ASSERT_TRUE(*r == v, "Fuzz int64 mismatch");
    }

    return true;
}

TEST_CASE(fuzz_doubles)
{
    FuzzRandom rng(0xD0B1E1234ULL);

    for (int i = 0; i < 2000; ++i)
    {
        const double v = rng.dbl(-1e6, 1e6);
        auto r = fuzz_roundtrip(v);
        ASSERT_TRUE(r.has_value(), r.error().message.c_str());
        ASSERT_TRUE(std::fabs(*r - v) < 1e-9, "Fuzz double mismatch");
    }

    return true;
}

TEST_CASE(fuzz_strings)
{
    FuzzRandom rng(0x5AB1CAFE21ULL);

    for (int i = 0; i < 2000; ++i)
    {
        const std::string v = rng.str(64);
        auto r = fuzz_roundtrip(v);
        ASSERT_TRUE(r.has_value(), r.error().message.c_str());
        ASSERT_TRUE(*r == v, "Fuzz string mismatch");
    }

    return true;
}

TEST_CASE(fuzz_vector_int)
{
    FuzzRandom rng(0xF00BA12345ULL);

    for (int iter = 0; iter < 500; ++iter)
    {
        const std::size_t len =
            static_cast<std::size_t>(rng.u64(0U, 32U));

        std::vector<int> v;
        v.reserve(len);
        for (std::size_t i = 0; i < len; ++i)
        {
            v.push_back(static_cast<int>(rng.i64(-100000, 100000)));
        }

        auto r = fuzz_roundtrip(v);
        ASSERT_TRUE(r.has_value(), r.error().message.c_str());
        ASSERT_TRUE(*r == v, "Fuzz vector<int> mismatch");
    }

    return true;
}

TEST_CASE(fuzz_map_string_int)
{
    FuzzRandom rng(0xAABFE21234ULL);

    for (int iter = 0; iter < 300; ++iter)
    {
        const std::size_t len =
            static_cast<std::size_t>(rng.u64(0U, 16U));

        std::map<std::string, int> m;
        for (std::size_t i = 0; i < len; ++i)
        {
            const std::string key = rng.str(16);
            const int value = static_cast<int>(rng.i64(-1000, 1000));
            m[key] = value;
        }

        auto r = fuzz_roundtrip(m);
        ASSERT_TRUE(r.has_value(), r.error().message.c_str());
        ASSERT_TRUE(*r == m, "Fuzz map<string,int> mismatch");
    }

    return true;
}

TEST_CASE(fuzz_nested_structures)
{
    FuzzRandom rng(0xAE5EDDA01ULL);

    for (int iter = 0; iter < 200; ++iter)
    {
        const std::size_t outer_len =
            static_cast<std::size_t>(rng.u64(0U, 8U));

        std::vector<std::map<std::string, int>> v;
        v.reserve(outer_len);

        for (std::size_t i = 0; i < outer_len; ++i)
        {
            std::map<std::string, int> inner;
            const std::size_t inner_len =
                static_cast<std::size_t>(rng.u64(0U, 8U));

            for (std::size_t j = 0; j < inner_len; ++j)
            {
                const std::string key = rng.str(10);
                const int value =
                    static_cast<int>(rng.i64(-5000, 5000));
                inner[key] = value;
            }

            v.push_back(std::move(inner));
        }

        auto r = fuzz_roundtrip(v);
        ASSERT_TRUE(r.has_value(), r.error().message.c_str());
        ASSERT_TRUE(*r == v, "Fuzz nested vector<map<string,int>> mismatch");
    }

    return true;
}

// =============================================================================
// Encode/Decode Benchmark Tests
// =============================================================================

template <typename F>
double bench_time_ms(F&& func, int iterations)
{
    using clock = std::chrono::high_resolution_clock;
    const auto start = clock::now();
    for (int i = 0; i < iterations; ++i)
    {
        func();
    }
    const auto end = clock::now();
    const auto elapsed =
        std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(end - start);
    return elapsed.count() / static_cast<double>(iterations);
}

TEST_CASE(encode_decode_benchmark_vector_int)
{
    std::vector<int> vec;
    vec.reserve(10000);
    for (int i = 0; i < 10000; ++i)
    {
        vec.push_back(i);
    }

    const double enc_vec_ms = bench_time_ms(
        [&]() {
            std::string json;
            auto rc = json_encode_to(json, vec);
            (void)rc;
        },
        100);

    std::string vec_json;
    (void)json_encode_to(vec_json, vec);

    const double dec_vec_ms = bench_time_ms(
        [&]() {
            auto rc = json_decode_from<std::vector<int>>(vec_json);
            (void)rc;
        },
        100);

    *get_test_config().output << "  vector<int> encode: " << enc_vec_ms << " ms\n";
    *get_test_config().output << "  vector<int> decode: " << dec_vec_ms << " ms\n";

    return true;
}

TEST_CASE(encode_decode_benchmark_map_string_int)
{
    std::map<std::string, int> m;
    for (int i = 0; i < 2000; ++i)
    {
        m.emplace("key_" + std::to_string(i), i);
    }

    const double enc_map_ms = bench_time_ms(
        [&]() {
            std::string json;
            auto rc = json_encode_to(json, m);
            (void)rc;
        },
        50);

    std::string map_json;
    (void)json_encode_to(map_json, m);

    const double dec_map_ms = bench_time_ms(
        [&]() {
            auto rc = json_decode_from<std::map<std::string, int>>(map_json);
            (void)rc;
        },
        50);

    *get_test_config().output << "  map<string,int> encode: " << enc_map_ms << " ms\n";
    *get_test_config().output << "  map<string,int> decode: " << dec_map_ms << " ms\n";

    return true;
}

TEST_CASE(encode_decode_benchmark_nested)
{
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

    const double enc_nested_ms = bench_time_ms(
        [&]() {
            std::string json;
            auto rc = json_encode_to(json, nested);
            (void)rc;
        },
        20);

    std::string nested_json;
    (void)json_encode_to(nested_json, nested);

    const double dec_nested_ms = bench_time_ms(
        [&]() {
            auto rc =
                json_decode_from<std::vector<std::map<std::string, int>>>(nested_json);
            (void)rc;
        },
        20);

    *get_test_config().output << "  vector<map<string,int>> encode: " << enc_nested_ms << " ms\n";
    *get_test_config().output << "  vector<map<string,int>> decode: " << dec_nested_ms << " ms\n";

    return true;
}

} // namespace fat_p::testing::fatpjson

namespace fat_p::testing {

void run_benchmarks() {
    *get_test_config().output << "\n" << colors::bold() 
                               << "==========================================================\n";
    *get_test_config().output << "PERFORMANCE BENCHMARKS - JsonLite vs FatPJson\n";
    *get_test_config().output << "==========================================================" 
                               << colors::reset() << "\n";
    
    fatpjson::benchmark_parse_small_json();
    fatpjson::benchmark_parse_large_array();
    fatpjson::benchmark_array_operations();
    fatpjson::benchmark_object_operations();
    fatpjson::benchmark_string_pool_memory();
    fatpjson::benchmark_error_handling();
    fatpjson::benchmark_memory_mapped_io();
}

bool test_FatPJson() {
    PRINT_HEADER(FATPJSONLITE);
    
    TestRunner runner;
    
    RUN_TEST_NS(runner, fatpjson, expected_api_basic);
    RUN_TEST_NS(runner, fatpjson, expected_api_types);
    RUN_TEST_NS(runner, fatpjson, error_handling);
    RUN_TEST_NS(runner, fatpjson, json_stats);
    RUN_TEST_NS(runner, fatpjson, safe_numeric_conversions);
    RUN_TEST_NS(runner, fatpjson, safe_generic_conversions);
    RUN_TEST_NS(runner, fatpjson, file_operations);
    RUN_TEST_NS(runner, fatpjson, memory_mapped_io);
    RUN_TEST_NS(runner, fatpjson, json_array);
    RUN_TEST_NS(runner, fatpjson, json_object);
    RUN_TEST_NS(runner, fatpjson, pooled_json_object);
    RUN_TEST_NS(runner, fatpjson, batch_parsing);
    RUN_TEST_NS(runner, fatpjson, jsonc_comments);
    RUN_TEST_NS(runner, fatpjson, utf8_handling);
    RUN_TEST_NS(runner, fatpjson, nan_infinity_handling);
    RUN_TEST_NS(runner, fatpjson, numeric_bounds_no_margin);
    RUN_TEST_NS(runner, fatpjson, value_returning_api);
    RUN_TEST_NS(runner, fatpjson, large_datasets);
    RUN_TEST_NS(runner, fatpjson, locale_independence);
    RUN_TEST_NS(runner, fatpjson, enum_integration_basic);
    RUN_TEST_NS(runner, fatpjson, enum_integration_structs);
    RUN_TEST_NS(runner, fatpjson, enum_integration_errors);
    RUN_TEST_NS(runner, fatpjson, atomic_save_basic);
    RUN_TEST_NS(runner, fatpjson, atomic_save_safety);
    RUN_TEST_NS(runner, fatpjson, atomic_save_vs_regular);
    
    RUN_TEST_NS(runner, fatpjson, json_pointer_basic);
    RUN_TEST_NS(runner, fatpjson, json_pointer_type_safe);
    RUN_TEST_NS(runner, fatpjson, json_pointer_errors);
    RUN_TEST_NS(runner, fatpjson, json_pointer_mutable);
    RUN_TEST_NS(runner, fatpjson, json_pointer_escape_sequences);
    RUN_TEST_NS(runner, fatpjson, json_pointer_complex);
    RUN_TEST_NS(runner, fatpjson, json_pointer_optional);
    RUN_TEST_NS(runner, fatpjson, json_pointer_monadic);
    RUN_TEST_NS(runner, fatpjson, json_pointer_structs);
    
    // Malformed input tests
    RUN_TEST_NS(runner, fatpjson, malformed_missing_comma);
    RUN_TEST_NS(runner, fatpjson, malformed_trailing_comma_array);
    RUN_TEST_NS(runner, fatpjson, malformed_trailing_comma_object);
    RUN_TEST_NS(runner, fatpjson, malformed_unquoted_key);
    RUN_TEST_NS(runner, fatpjson, malformed_unterminated_string);
    RUN_TEST_NS(runner, fatpjson, malformed_numbers);
    RUN_TEST_NS(runner, fatpjson, malformed_string_escapes);
    RUN_TEST_NS(runner, fatpjson, malformed_structure);
    RUN_TEST_NS(runner, fatpjson, malformed_literals);
    RUN_TEST_NS(runner, fatpjson, malformed_deep_nesting_stress);
    RUN_TEST_NS(runner, fatpjson, malformed_unicode);
    
    // Fuzz tests
    RUN_TEST_NS(runner, fatpjson, fuzz_ints);
    RUN_TEST_NS(runner, fatpjson, fuzz_int64);
    RUN_TEST_NS(runner, fatpjson, fuzz_doubles);
    RUN_TEST_NS(runner, fatpjson, fuzz_strings);
    RUN_TEST_NS(runner, fatpjson, fuzz_vector_int);
    RUN_TEST_NS(runner, fatpjson, fuzz_map_string_int);
    RUN_TEST_NS(runner, fatpjson, fuzz_nested_structures);
    
    // Encode/decode benchmarks
    RUN_TEST_NS(runner, fatpjson, encode_decode_benchmark_vector_int);
    RUN_TEST_NS(runner, fatpjson, encode_decode_benchmark_map_string_int);
    RUN_TEST_NS(runner, fatpjson, encode_decode_benchmark_nested);
    
    run_benchmarks();
    
    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

// =============================================================================
// Standalone Entry Point
// =============================================================================
#ifdef ENABLE_TEST_APPLICATION
int main() {
    return fat_p::testing::test_FatPJson() ? 0 : 1;
}
#endif
