/**
 * @file test_FatPJsonLite.cpp
 * @brief Comprehensive unit tests for FatPJsonLite.h
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

#include "FatPJsonLite.h"
#include "FatPTest.h"

#ifndef ENABLE_TEST_APPLICATION
#include "test_FatPJsonLite.h"
#endif


namespace fat_p::testing {

USING_FATP_JSON_LITE()

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

namespace fat_p::testing {

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

bool test_fpjl_expected_api_basic() {
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
    
    return true;
}

bool test_fpjl_expected_api_types() {
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
    
    return true;
}

bool test_fpjl_error_handling() {
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
    
    return true;
}

bool test_fpjl_safe_numeric_conversions() {
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
    
    return true;
}

bool test_fpjl_safe_generic_conversions() {
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
    
    return true;
}

bool test_fpjl_file_operations() {
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
    
    return true;
}

bool test_fpjl_memory_mapped_io() {
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
    
    return true;
}

bool test_fpjl_json_array() {
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
    
    return true;
}

bool test_fpjl_json_object() {
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
    
    return true;
}

bool test_fpjl_pooled_json_object() {
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
    
    return true;
}

bool test_fpjl_batch_parsing() {
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
    
    return true;
}

bool test_fpjl_jsonc_comments() {
    SUBTEST("Line comments") {
        std::string json = R"({
            "port": 8080,
            "host": "localhost"
        })";
        
        auto result = try_parse_json<ConfigJsonPolicy>(json);
        ASSERT_TRUE(result.has_value(), "Should parse with line comments");
        
        const auto& obj = std::get<JsonObject>(*result);
        ASSERT_EQ(from_json<int>(obj, "port"), 8080, "Port should be 8080");
    }
    END_SUBTEST
    
    SUBTEST("Block comments") {
        std::string json = R"({
            "name": "test",
            "value": 42
        })";
        
        auto result = try_parse_json<ConfigJsonPolicy>(json);
        ASSERT_TRUE(result.has_value(), "Should parse with block comments");
    }
    END_SUBTEST
    
    SUBTEST("Strict policy rejects comments") {
        std::string json = R"({
            "key": "value"
        })";
        
        auto result = try_parse_json<StandardJsonPolicy>(json);
        ASSERT_FALSE(result.has_value(), "Standard policy should reject comments");
        ASSERT_EQ(result.error().code, JsonErrorCode::ParseError, "Should be parse error");
    }
    END_SUBTEST
    
    return true;
}

bool test_fpjl_utf8_handling() {
    SUBTEST("European chars escaped") {
        std::string text = "cafÃƒÂ©";
        std::string json = to_json_string<std::string, StandardJsonPolicy>(text);
        
        ASSERT_TRUE(json.find("\\u") != std::string::npos, "Should escape European chars");
        
        auto parsed = try_parse_json(json);
        ASSERT_TRUE(parsed.has_value(), "Should parse back");
        ASSERT_EQ(std::get<std::string>(*parsed), text, "Should round-trip");
    }
    END_SUBTEST
    
    SUBTEST("Asian chars escaped") {
        std::string text = "Ã¤Â¸â€“Ã§â€¢Å’";
        std::string json = to_json_string<std::string, StandardJsonPolicy>(text);
        
        ASSERT_TRUE(json.find("\\u") != std::string::npos, "Should escape Asian chars");
        
        auto parsed = try_parse_json(json);
        ASSERT_TRUE(parsed.has_value(), "Should parse back");
    }
    END_SUBTEST
    
    SUBTEST("Emoji with surrogate pairs") {
        std::string text = "Ã°Å¸Ëœâ‚¬";
        std::string json = to_json_string<std::string, StandardJsonPolicy>(text);
        
        ASSERT_TRUE(json.find("\\u") != std::string::npos, "Should escape emoji");
        
        auto parsed = try_parse_json(json);
        ASSERT_TRUE(parsed.has_value(), "Should parse back");
    }
    END_SUBTEST
    
    SUBTEST("Raw UTF-8 with CompatJsonPolicy") {
        std::string text = "Ã¤Â¸â€“Ã§â€¢Å’ cafÃƒÂ© Ã°Å¸Ëœâ‚¬";
        std::string json = to_json_string<std::string, CompatJsonPolicy>(text);
        
        ASSERT_TRUE(json.find("\\u") == std::string::npos, "CompatPolicy should not escape");
        
        auto parsed = try_parse_json(json);
        ASSERT_TRUE(parsed.has_value(), "Should parse raw UTF-8");
    }
    END_SUBTEST
    
    return true;
}

bool test_fpjl_nan_infinity_handling() {
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
    
    return true;
}

bool test_fpjl_numeric_bounds_no_margin() {
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
    
    return true;
}

bool test_fpjl_value_returning_api() {
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
    
    return true;
}

bool test_fpjl_large_datasets() {
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
    
    return true;
}

bool test_fpjl_enum_integration_basic() {
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
    
    return true;
}

bool test_fpjl_enum_integration_structs() {
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
    
    return true;
}

bool test_fpjl_enum_integration_errors() {
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
    
    return true;
}

bool test_fpjl_atomic_save_basic() {
    SUBTEST("Atomic save creates file") {
        const std::string filename = "test_atomic_basic.json";
        JsonValue j = to_json(42);
        
        auto result = try_save_atomic(filename, j);
        
        ASSERT_TRUE(result.has_value(), "Should save successfully");
        ASSERT_TRUE(std::filesystem::exists(filename), "File should exist");
        
        std::filesystem::remove(filename);
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
        
        std::filesystem::remove(filename);
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
        
        std::filesystem::remove(filename);
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
        
        std::filesystem::remove(filename);
    }
    END_SUBTEST
    
    return true;
}

bool test_fpjl_atomic_save_safety() {
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
        
        std::filesystem::remove(filename);
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
        
        std::filesystem::remove(filename);
    }
    END_SUBTEST
    
    return true;
}

bool test_fpjl_atomic_save_vs_regular() {
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
        
        std::filesystem::remove(atomic_file);
        std::filesystem::remove(regular_file);
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
        
        std::filesystem::remove(filename);
    }
    END_SUBTEST
    
    return true;
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
    *get_test_config().output << "  FatPJsonLite:  " << format_time(fatp_time) << "\n";
    
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
    *get_test_config().output << "  FatPJsonLite:  " << format_time(fatp_time) << "\n";
    
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

bool test_fpjl_json_pointer_basic() {
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
    
    return true;
}

bool test_fpjl_json_pointer_type_safe() {
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
    
    return true;
}

bool test_fpjl_json_pointer_errors() {
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
    
    return true;
}

bool test_fpjl_json_pointer_mutable() {
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
    
    return true;
}

bool test_fpjl_json_pointer_escape_sequences() {
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
    
    return true;
}

bool test_fpjl_json_pointer_complex() {
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
    
    return true;
}

bool test_fpjl_json_pointer_optional() {
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
    
    return true;
}

bool test_fpjl_json_pointer_monadic() {
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
    
    return true;
}

bool test_fpjl_json_pointer_structs() {
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
    
    return true;
}

void run_benchmarks() {
    *get_test_config().output << "\n" << colors::bold() 
                               << "==========================================================\n";
    *get_test_config().output << "PERFORMANCE BENCHMARKS - JsonLite vs FatPJsonLite\n";
    *get_test_config().output << "==========================================================" 
                               << colors::reset() << "\n";
    
    benchmark_parse_small_json();
    benchmark_parse_large_array();
    benchmark_array_operations();
    benchmark_object_operations();
    benchmark_string_pool_memory();
    benchmark_error_handling();
    benchmark_memory_mapped_io();
}

bool test_FatPJsonLite() {
    PRINT_HEADER(FATPJSONLITE);
    
    TestRunner runner;
    
    RUN_TEST(runner, fpjl_expected_api_basic);
    RUN_TEST(runner, fpjl_expected_api_types);
    RUN_TEST(runner, fpjl_error_handling);
    RUN_TEST(runner, fpjl_safe_numeric_conversions);
    RUN_TEST(runner, fpjl_safe_generic_conversions);
    RUN_TEST(runner, fpjl_file_operations);
    RUN_TEST(runner, fpjl_memory_mapped_io);
    RUN_TEST(runner, fpjl_json_array);
    RUN_TEST(runner, fpjl_json_object);
    RUN_TEST(runner, fpjl_pooled_json_object);
    RUN_TEST(runner, fpjl_batch_parsing);
    RUN_TEST(runner, fpjl_jsonc_comments);
    RUN_TEST(runner, fpjl_utf8_handling);
    RUN_TEST(runner, fpjl_nan_infinity_handling);
    RUN_TEST(runner, fpjl_numeric_bounds_no_margin);
    RUN_TEST(runner, fpjl_value_returning_api);
    RUN_TEST(runner, fpjl_large_datasets);
    RUN_TEST(runner, fpjl_enum_integration_basic);
    RUN_TEST(runner, fpjl_enum_integration_structs);
    RUN_TEST(runner, fpjl_enum_integration_errors);
    RUN_TEST(runner, fpjl_atomic_save_basic);
    RUN_TEST(runner, fpjl_atomic_save_safety);
    RUN_TEST(runner, fpjl_atomic_save_vs_regular);
    
    RUN_TEST(runner, fpjl_json_pointer_basic);
    RUN_TEST(runner, fpjl_json_pointer_type_safe);
    RUN_TEST(runner, fpjl_json_pointer_errors);
    RUN_TEST(runner, fpjl_json_pointer_mutable);
    RUN_TEST(runner, fpjl_json_pointer_escape_sequences);
    RUN_TEST(runner, fpjl_json_pointer_complex);
    RUN_TEST(runner, fpjl_json_pointer_optional);
    RUN_TEST(runner, fpjl_json_pointer_monadic);
    RUN_TEST(runner, fpjl_json_pointer_structs);
    
    run_benchmarks();
    
    return 0 == runner.print_summary();
}

}

#ifdef ENABLE_TEST_APPLICATION
int main() {
    return fat_p::testing::test_FatPJsonLite() ? 0 : 1;
}
#endif
