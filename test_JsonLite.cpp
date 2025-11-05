// test_JsonLite.cpp
#include "JsonLite.h"
#include "test_JsonLite.h"
#include "test_Utilities.h"
#include <cassert>
#include <sstream>
#include <cmath>
#include <climits>


// ====================================================================
// Test Macros
// ====================================================================

#define TEST_SECTION(name) \
    std::cout << colors::cyan() << "\n=== " << name << " ===" << colors::reset() << std::endl

#define TEST_PASS(msg) \
    std::cout << colors::green() << "✓ " << msg << colors::reset() << std::endl; \
    return true

#define TEST_SUITE_BEGIN(name) \
    std::cout << colors::bold() << "\n" << std::string(60, '=') << "\n" \
              << name << "\n" << std::string(60, '=') << colors::reset() << std::endl

#define TEST_SUITE_END() \
    std::cout << colors::bold() << "\n" << std::string(60, '=') << "\n" \
              << "All Tests Completed\n" << std::string(60, '=') << colors::reset() << std::endl

// ====================================================================
// Test Structs
// ====================================================================

namespace cpp_utilities {

struct Point {
    int x;
    int y;
};
CPP_JSON_DEFINE_TYPE_NON_INTRUSIVE(Point, x, y)

struct Person {
    std::string name;
    int age;
    std::vector<std::string> hobbies;
};
CPP_JSON_DEFINE_TYPE_NON_INTRUSIVE(Person, name, age, hobbies)

struct Config {
    std::optional<int> timeout;
    std::optional<std::string> host;
    int port = 8080;
};
CPP_JSON_DEFINE_TYPE_OPTIONAL(Config, timeout, host, port)

class PrivateData {
private:
    int secret_;
    std::string code_;
public:
    PrivateData() : secret_(0), code_() {}
    PrivateData(int s, std::string c) : secret_(s), code_(std::move(c)) {}
    
    int secret() const { return secret_; }
    std::string code() const { return code_; }
    
    CPP_JSON_DEFINE_TYPE_INTRUSIVE(PrivateData, secret_, code_)
};

struct Nested {
    Point position;
    std::string label;
};
CPP_JSON_DEFINE_TYPE_NON_INTRUSIVE(Nested, position, label)

struct Complex {
    std::map<std::string, int> scores;
    std::vector<Point> points;
    std::optional<std::string> description;
};
CPP_JSON_DEFINE_TYPE_NON_INTRUSIVE(Complex, scores, points, description)

// Test struct for param helpers
struct AppConfig {
    int port;
    std::string host;
    std::vector<std::string> allowed_ips;
    std::optional<int> timeout;
};
CPP_JSON_DEFINE_TYPE_NON_INTRUSIVE(AppConfig, port, host, allowed_ips, timeout)

}  // namespace cpp_utilities

// ====================================================================
// Basic Type Tests
// ====================================================================

namespace cpp_utilities::testing
{

bool test_basic_types() {
    TEST_SECTION("Basic JSON Types");
    
    // Null
    {
        JsonValue j = nullptr;
        assert(j.is_null());
        std::string str = to_json_string(j);
        assert(str == "null");
    }
    
    // Boolean
    {
        JsonValue j_true = true;
        JsonValue j_false = false;
        assert(j_true.is_bool());
        assert(to_json_string(j_true) == "true");
        assert(to_json_string(j_false) == "false");
    }
    
    // Numbers
    {
        JsonValue j_int = 42.0;
        JsonValue j_float = 3.14159;
        assert(j_int.is_number());
        assert(to_json_string(j_int) == "42");
        assert(to_json_string(j_float).substr(0, 4) == "3.14");
    }
    
    // Strings
    {
        JsonValue j = std::string("hello");
        assert(j.is_string());
        assert(to_json_string(j) == "\"hello\"");
    }
    
    TEST_PASS("Basic types serialize correctly");
}

bool test_string_escaping() {
    TEST_SECTION("String Escaping");
    
    std::string test_str = "line1\nline2\ttab\"quote\\backslash";
    JsonValue j = test_str;
    std::string result = to_json_string(j);
    
    assert(result.find("\\n") != std::string::npos);
    assert(result.find("\\t") != std::string::npos);
    assert(result.find("\\\"") != std::string::npos);
    assert(result.find("\\\\") != std::string::npos);
    
    TEST_PASS("String escaping works correctly");
}

bool test_containers() {
    TEST_SECTION("Container Serialization");
    
    // Vector
    {
        std::vector<int> vec = {1, 2, 3, 4, 5};
        JsonValue j = to_json(vec);
        assert(j.is_array());
        std::string str = to_json_string(j);
        assert(str == "[1,2,3,4,5]");
        
        std::vector<int> vec2;
        from_json(j, vec2);
        assert(vec == vec2);
    }
    
    // Map
    {
        std::map<std::string, int> map = {{"a", 1}, {"b", 2}};
        JsonValue j = to_json(map);
        assert(j.is_object());
        
        std::map<std::string, int> map2;
        from_json(j, map2);
        assert(map == map2);
    }
    
    // Nested containers
    {
        std::vector<std::vector<int>> nested = {{1, 2}, {3, 4}};
        JsonValue j = to_json(nested);
        std::string str = to_json_string(j);
        assert(str == "[[1,2],[3,4]]");
    }
    
    TEST_PASS("Container serialization works");
}

bool test_optional() {
    TEST_SECTION("Optional Support");
    
    // Has value
    {
        std::optional<int> opt = 42;
        JsonValue j = to_json(opt);
        assert(!j.is_null());
        
        std::optional<int> opt2;
        from_json(j, opt2);
        assert(opt2.has_value());
        assert(*opt2 == 42);
    }
    
    // No value
    {
        std::optional<int> opt;
        JsonValue j = to_json(opt);
        assert(j.is_null());
        
        std::optional<int> opt2 = 99;
        from_json(j, opt2);
        assert(!opt2.has_value());
    }
    
    TEST_PASS("Optional serialization works");
}

bool test_tuples_pairs() {
    TEST_SECTION("Tuple and Pair Serialization");
    
    // Pair
    {
        std::pair<int, std::string> p = {42, "answer"};
        JsonValue j = to_json(p);
        assert(j.is_array());
        
        std::pair<int, std::string> p2;
        from_json(j, p2);
        assert(p == p2);
    }
    
    // Tuple
    {
        std::tuple<int, std::string, bool> t = {1, "test", true};
        JsonValue j = to_json(t);
        std::string str = to_json_string(j);
        assert(str == "[1,\"test\",true]");
        
        std::tuple<int, std::string, bool> t2;
        from_json(j, t2);
        assert(t == t2);
    }
    
    TEST_PASS("Tuple/pair serialization works");
}

// ====================================================================
// Struct Tests
// ====================================================================

bool test_simple_struct() {
    TEST_SECTION("Simple Struct Serialization");
    
    Point p{10, 20};
    JsonValue j = to_json(p);
    
    std::string str = to_json_string(j);
    assert(str.find("\"x\"") != std::string::npos);
    assert(str.find("10") != std::string::npos);
    assert(str.find("\"y\"") != std::string::npos);
    assert(str.find("20") != std::string::npos);
    
    Point p2;
    from_json(j, p2);
    assert(p2.x == 10);
    assert(p2.y == 20);
    
    TEST_PASS("Simple struct serialization works");
}

bool test_complex_struct() {
    TEST_SECTION("Complex Struct Serialization");
    
    Person person{"Alice", 30, {"reading", "coding", "hiking"}};
    JsonValue j = to_json(person);
    
    Person person2;
    from_json(j, person2);
    
    assert(person2.name == "Alice");
    assert(person2.age == 30);
    assert(person2.hobbies.size() == 3);
    assert(person2.hobbies[0] == "reading");
    
    TEST_PASS("Complex struct with nested containers works");
}

bool test_optional_fields() {
    TEST_SECTION("Optional Fields in Structs");
    
    // All fields present
    {
        Config cfg;
        cfg.timeout = 5000;
        cfg.host = "localhost";
        cfg.port = 9090;
        
        JsonValue j = to_json(cfg);
        Config cfg2;
        from_json(j, cfg2);
        
        assert(cfg2.timeout.has_value());
        assert(*cfg2.timeout == 5000);
        assert(cfg2.host.has_value());
        assert(*cfg2.host == "localhost");
        assert(cfg2.port == 9090);
    }
    
    // Optional fields missing
    {
        std::string json = R"({"port": 3000})";
        JsonValue j = parse_json(json);
        Config cfg;
        from_json(j, cfg);
        
        assert(!cfg.timeout.has_value());
        assert(!cfg.host.has_value());
        assert(cfg.port == 3000);
    }
    
    TEST_PASS("Optional field handling works");
}

bool test_intrusive_serialization() {
    TEST_SECTION("Intrusive Serialization (Private Members)");
    
    PrivateData data(42, "secret123");
    JsonValue j = to_json(data);
    
    PrivateData data2;
    from_json(j, data2);
    
    assert(data2.secret() == 42);
    assert(data2.code() == "secret123");
    
    TEST_PASS("Intrusive serialization with private members works");
}

bool test_nested_structs() {
    TEST_SECTION("Nested Struct Serialization");
    
    Nested nested{{5, 10}, "origin"};
    JsonValue j = to_json(nested);
    
    std::string str = to_json_string(j);
    assert(str.find("\"position\"") != std::string::npos);
    assert(str.find("\"label\"") != std::string::npos);
    
    Nested nested2;
    from_json(j, nested2);
    
    assert(nested2.position.x == 5);
    assert(nested2.position.y == 10);
    assert(nested2.label == "origin");
    
    TEST_PASS("Nested struct serialization works");
}

bool test_complex_nested() {
    TEST_SECTION("Complex Nested Structures");
    
    Complex c;
    c.scores["math"] = 95;
    c.scores["science"] = 88;
    c.points = {{1, 2}, {3, 4}, {5, 6}};
    c.description = "Test data";
    
    JsonValue j = to_json(c);
    
    Complex c2;
    from_json(j, c2);
    
    assert(c2.scores.size() == 2);
    assert(c2.scores["math"] == 95);
    assert(c2.points.size() == 3);
    assert(c2.points[0].x == 1);
    assert(c2.description.has_value());
    assert(*c2.description == "Test data");
    
    TEST_PASS("Complex nested structures work");
}

// ====================================================================
// Parser Tests
// ====================================================================

bool test_parser_basic() {
    TEST_SECTION("JSON Parser - Basic Types");
    
    // Parse null
    {
        JsonValue j = parse_json("null");
        assert(j.is_null());
    }
    
    // Parse boolean
    {
        JsonValue j_true = parse_json("true");
        JsonValue j_false = parse_json("false");
        assert(std::get<bool>(j_true) == true);
        assert(std::get<bool>(j_false) == false);
    }
    
    // Parse number
    {
        JsonValue j = parse_json("42.5");
        assert(std::abs(std::get<double>(j) - 42.5) < 0.001);
    }
    
    // Parse string
    {
        JsonValue j = parse_json("\"hello world\"");
        assert(std::get<std::string>(j) == "hello world");
    }
    
    TEST_PASS("Parser handles basic types");
}

bool test_parser_containers() {
    TEST_SECTION("JSON Parser - Containers");
    
    // Parse array
    {
        JsonValue j = parse_json("[1, 2, 3]");
        assert(j.is_array());
        const auto& arr = std::get<JsonArray>(j);
        assert(arr.size() == 3);
        assert(std::get<double>(arr[0]) == 1);
    }
    
    // Parse object
    {
        JsonValue j = parse_json(R"({"name": "Alice", "age": 30})");
        assert(j.is_object());
        const auto& obj = std::get<JsonObject>(j);
        assert(obj.size() == 2);
        assert(std::get<std::string>(obj.at("name")) == "Alice");
        assert(std::get<double>(obj.at("age")) == 30);
    }
    
    // Parse nested
    {
        JsonValue j = parse_json(R"({"items": [1, 2, {"nested": true}]})");
        assert(j.is_object());
        const auto& obj = std::get<JsonObject>(j);
        const auto& arr = std::get<JsonArray>(obj.at("items"));
        assert(arr.size() == 3);
        const auto& nested = std::get<JsonObject>(arr[2]);
        assert(std::get<bool>(nested.at("nested")) == true);
    }
    
    TEST_PASS("Parser handles containers");
}

bool test_parser_edge_cases() {
    TEST_SECTION("JSON Parser - Edge Cases");
    
    // Empty array
    {
        JsonValue j = parse_json("[]");
        assert(j.is_array());
        assert(std::get<JsonArray>(j).empty());
    }
    
    // Empty object
    {
        JsonValue j = parse_json("{}");
        assert(j.is_object());
        assert(std::get<JsonObject>(j).empty());
    }
    
    // Whitespace handling
    {
        JsonValue j = parse_json("  \n\t{ \"key\" : \"value\" }\n  ");
        assert(j.is_object());
        const auto& obj = std::get<JsonObject>(j);
        assert(std::get<std::string>(obj.at("key")) == "value");
    }
    
    TEST_PASS("Parser handles edge cases");
}

bool test_parser_errors() {
    TEST_SECTION("JSON Parser - Error Handling");
    
    bool caught = false;
    
    // Invalid JSON
    try {
        (void)parse_json("invalid");
    } catch (const std::runtime_error&) {
        caught = true;
    }
    assert(caught);
    
    // Unterminated string
    caught = false;
    try {
        (void)parse_json("\"unterminated");
    } catch (const std::runtime_error&) {
        caught = true;
    }
    assert(caught);
    
    // Extra data
    caught = false;
    try {
        (void)parse_json("42 extra");
    } catch (const std::runtime_error&) {
        caught = true;
    }
    assert(caught);
    
    TEST_PASS("Parser error handling works");
}

// ====================================================================
// Policy Tests
// ====================================================================

bool test_pretty_print() {
    TEST_SECTION("Pretty Print Policy");
    
    Point p{10, 20};
    std::string compact = to_json_string<Point, StandardJsonPolicy>(p, false);
    std::string pretty = to_json_string<Point, PrettyJsonPolicy>(p, true);
    
    assert(compact.find('\n') == std::string::npos);
    assert(pretty.find('\n') != std::string::npos);
    assert(pretty.size() > compact.size());
    
    TEST_PASS("Pretty print policy works");
}

bool test_numeric_precision() {
    TEST_SECTION("Numeric Precision Policy");
    
    double pi = 3.14159265359;
    JsonValue j = pi;
    
    std::string str = to_json_string(j);
    // Default precision is 6
    assert(str.find("3.14159") != std::string::npos);
    
    TEST_PASS("Numeric precision policy works");
}

bool test_nan_inf_handling() {
    TEST_SECTION("NaN/Infinity Handling");
    
    // Standard policy - converts to null
    {
        double nan_val = std::numeric_limits<double>::quiet_NaN();
        JsonValue j = nan_val;
        std::string str = to_json_string<JsonValue, StandardJsonPolicy>(j);
        assert(str == "null");
    }
    
    // Compat policy - keeps NaN/Inf
    {
        double inf_val = std::numeric_limits<double>::infinity();
        JsonValue j = inf_val;
        std::string str = to_json_string<JsonValue, CompatJsonPolicy>(j);
        assert(str == "Infinity");
    }
    
    TEST_PASS("NaN/Infinity handling works");
}

// ====================================================================
// File I/O Tests
// ====================================================================

bool test_file_io() {
    TEST_SECTION("File I/O");
    
    const std::string filename = "test_json_output.json";
    
    // Write
    {
        Person person{"Bob", 25, {"music", "sports"}};
        JsonValue j = to_json(person);
        save_json_to_file(filename, j, false);
    }
    
    // Read
    {
        JsonValue j = load_json_from_file(filename);
        Person person;
        from_json(j, person);
        
        assert(person.name == "Bob");
        assert(person.age == 25);
        assert(person.hobbies.size() == 2);
    }
    
    // Cleanup
    std::remove(filename.c_str());
    
    TEST_PASS("File I/O works");
}

// ====================================================================
// Round-trip Tests
// ====================================================================

bool test_roundtrip_all_types() {
    TEST_SECTION("Round-trip All Types");
    
    // Test various types can round-trip correctly
    {
        int val = 42;
        JsonValue j = to_json(val);
        int val2 = from_json<int>(j);
        assert(val == val2);
    }
    
    {
        std::string val = "test string";
        JsonValue j = to_json(val);
        std::string val2 = from_json<std::string>(j);
        assert(val == val2);
    }
    
    {
        std::vector<int> val = {1, 2, 3, 4, 5};
        JsonValue j = to_json(val);
        std::vector<int> val2 = from_json<std::vector<int>>(j);
        assert(val == val2);
    }
    
    {
        std::map<std::string, int> val = {{"a", 1}, {"b", 2}};
        JsonValue j = to_json(val);
        std::map<std::string, int> val2 = from_json<std::map<std::string, int>>(j);
        assert(val == val2);
    }
    
    TEST_PASS("Round-trip for all types works");
}

bool test_convenience_functions() {
    TEST_SECTION("Convenience Functions");
    
    Point p{15, 25};
    
    // to_json_string / from_json_string
    std::string json_str = to_json_string(p);
    Point p2 = from_json_string<Point>(json_str);
    
    assert(p2.x == 15);
    assert(p2.y == 25);
    
    TEST_PASS("Convenience functions work");
}

// ====================================================================
// Performance/Stress Tests
// ====================================================================

bool test_large_data() {
    TEST_SECTION("Large Data Handling");
    
    // Large array
    std::vector<int> large_vec;
    large_vec.reserve(10000);
    for (int i = 0; i < 10000; ++i) {
        large_vec.push_back(i);
    }
    
    JsonValue j = to_json(large_vec);
    std::vector<int> large_vec2 = from_json<std::vector<int>>(j);
    
    assert(large_vec2.size() == 10000);
    assert(large_vec2[0] == 0);
    assert(large_vec2[9999] == 9999);
    
    TEST_PASS("Large data handling works");
}

bool test_deeply_nested() {
    TEST_SECTION("Deeply Nested Structures");
    
    // Create nested structure (within safe limits)
    JsonValue j = JsonObject{};
    auto* current = &std::get<JsonObject>(j);
    
    for (int i = 0; i < 50; ++i) {
        (*current)["nested"] = JsonObject{};
        current = &std::get<JsonObject>((*current)["nested"]);
    }
    (*current)["value"] = 42.0;
    
    // Serialize and parse
    std::string str = to_json_string(j);
    JsonValue j2 = parse_json(str);
    
    // Navigate back down
    const JsonObject* curr2 = &std::get<JsonObject>(j2);
    for (int i = 0; i < 50; ++i) {
        curr2 = &std::get<JsonObject>(curr2->at("nested"));
    }
    
    assert(std::get<double>(curr2->at("value")) == 42.0);
    
    TEST_PASS("Deeply nested structures work");
}

// ====================================================================
// SuperGrok Enhancement Tests
// ====================================================================

bool test_depth_limit_parse() {
    TEST_SECTION("Depth Limit - Parser");
    
    // Create deeply nested JSON (exceeds limit)
    std::string json = "[";
    for (int i = 0; i < 600; ++i) {
        json += "[";
    }
    json += "1";
    for (int i = 0; i < 600; ++i) {
        json += "]";
    }
    json += "]";
    
    bool caught = false;
    try {
        (void)parse_json(json);
    } catch (const std::runtime_error& e) {
        std::string msg = e.what();
        if (msg.find("maximum nesting depth") != std::string::npos) {
            caught = true;
        }
    }
    
    assert(caught);
    TEST_PASS("Parse depth limit protection works");
}

bool test_depth_limit_dump() {
    TEST_SECTION("Depth Limit - Serializer");
    
    // Create deeply nested structure (exceeds limit)
    JsonValue j = 1.0;
    for (int i = 0; i < 600; ++i) {
        JsonArray arr;
        arr.push_back(j);
        j = arr;
    }
    
    bool caught = false;
    try {
        (void)to_json_string(j);
    } catch (const std::runtime_error& e) {
        std::string msg = e.what();
        if (msg.find("maximum nesting depth") != std::string::npos) {
            caught = true;
        }
    }
    
    assert(caught);
    TEST_PASS("Dump depth limit protection works");
}

bool test_range_checks() {
    TEST_SECTION("Integer Range Checking");
    
    // Test int overflow
    {
        JsonValue j = static_cast<double>(INT_MAX) + 1e10;
        int value = 0;
        bool caught = false;
        try {
            from_json(j, value);
        } catch (const std::runtime_error& e) {
            std::string msg = e.what();
            if (msg.find("out of range") != std::string::npos) {
                caught = true;
            }
        }
        assert(caught);
    }
    
    // Test unsigned underflow
    {
        JsonValue j = -1.0;
        unsigned int value = 0;
        bool caught = false;
        try {
            from_json(j, value);
        } catch (const std::runtime_error& e) {
            std::string msg = e.what();
            if (msg.find("out of range") != std::string::npos) {
                caught = true;
            }
        }
        assert(caught);
    }
    
    // Test valid conversion
    {
        JsonValue j = 42.0;
        int value = 0;
        from_json(j, value);
        assert(value == 42);
    }
    
    TEST_PASS("Range checking works");
}

bool test_better_error_messages() {
    TEST_SECTION("Enhanced Error Messages");
    
    // Test missing field error includes field name
    {
        std::string json = R"({"port": 8080, "host": "localhost"})"; // missing allowed_ips
        bool caught = false;
        try {
            (void)from_json_string<AppConfig>(json);
        } catch (const std::runtime_error& e) {
            std::string error_msg = e.what();
            if (error_msg.find("allowed_ips") != std::string::npos) {
                caught = true;
            }
        }
        assert(caught);
    }
    
    // Test type mismatch error includes field name
    {
        std::string json = R"({"port": "not a number", "host": "localhost", "allowed_ips": []})";
        bool caught = false;
        try {
            (void)from_json_string<AppConfig>(json);
        } catch (const std::runtime_error& e) {
            std::string error_msg = e.what();
            if (error_msg.find("port") != std::string::npos) {
                caught = true;
            }
        }
        assert(caught);
    }
    
    TEST_PASS("Enhanced error messages work");
}

bool test_param_helpers() {
    TEST_SECTION("Parameter Helper Functions");
    
    AppConfig config;
    config.port = 8080;
    config.host = "127.0.0.1";
    config.allowed_ips = {"192.168.1.1", "10.0.0.1"};
    config.timeout = 30;
    
    const std::string filename = "test_config_helpers.json";
    
    // Test save_params
    save_params(filename, config);
    
    // Test load_params
    AppConfig loaded = load_params<AppConfig>(filename);
    
    assert(loaded.port == config.port);
    assert(loaded.host == config.host);
    assert(loaded.allowed_ips == config.allowed_ips);
    assert(loaded.timeout == config.timeout);
    
    // Test save_params_with_backup
    config.port = 9090;
    save_params_with_backup(filename, config);
    
    // Check backup exists
    std::ifstream backup(filename + ".bak");
    assert(backup.good());
    backup.close();
    
    // Verify new config
    AppConfig loaded2 = load_params<AppConfig>(filename);
    assert(loaded2.port == 9090);
    
    // Cleanup
    std::remove(filename.c_str());
    std::remove((filename + ".bak").c_str());
    
    TEST_PASS("Param helper functions work");
}

bool test_position_in_errors() {
    TEST_SECTION("Position Information in Errors");
    
    std::string json = R"({"key": "value", "bad": })";
    bool caught = false;
    try {
        (void)parse_json(json);
    } catch (const std::runtime_error& e) {
        std::string error_msg = e.what();
        if (error_msg.find("position") != std::string::npos) {
            caught = true;
        }
    }
    assert(caught);
    
    TEST_PASS("Position information in errors works");
}


bool test_JsonLite() {
    TEST_SUITE_BEGIN("JSON Serialization Tests");
    
    TestRunner runner;
    auto& out = *get_test_config().output;
    
    // Basic types
    out << "\n" << colors::bold() << "=== Basic Type Tests ===" 
        << colors::reset() << std::endl;
    runner.run_test("Basic Types", test_basic_types);
    runner.run_test("String Escaping", test_string_escaping);
    runner.run_test("Containers", test_containers);
    runner.run_test("Optional Support", test_optional);
    runner.run_test("Tuples and Pairs", test_tuples_pairs);
    
    // Structs
    out << "\n" << colors::bold() << "=== Struct Serialization Tests ===" 
        << colors::reset() << std::endl;
    runner.run_test("Simple Struct", test_simple_struct);
    runner.run_test("Complex Struct", test_complex_struct);
    runner.run_test("Optional Fields", test_optional_fields);
    runner.run_test("Intrusive Serialization", test_intrusive_serialization);
    runner.run_test("Nested Structs", test_nested_structs);
    runner.run_test("Complex Nested", test_complex_nested);
    
    // Parser
    out << "\n" << colors::bold() << "=== Parser Tests ===" 
        << colors::reset() << std::endl;
    runner.run_test("Parser Basic", test_parser_basic);
    runner.run_test("Parser Containers", test_parser_containers);
    runner.run_test("Parser Edge Cases", test_parser_edge_cases);
    runner.run_test("Parser Errors", test_parser_errors);
    
    // Policies
    out << "\n" << colors::bold() << "=== Policy Tests ===" 
        << colors::reset() << std::endl;
    runner.run_test("Pretty Print", test_pretty_print);
    runner.run_test("Numeric Precision", test_numeric_precision);
    runner.run_test("NaN/Inf Handling", test_nan_inf_handling);
    
    // File I/O
    out << "\n" << colors::bold() << "=== File I/O Tests ===" 
        << colors::reset() << std::endl;
    runner.run_test("File I/O", test_file_io);
    
    // Round-trip
    out << "\n" << colors::bold() << "=== Round-trip Tests ===" 
        << colors::reset() << std::endl;
    runner.run_test("Roundtrip All Types", test_roundtrip_all_types);
    runner.run_test("Convenience Functions", test_convenience_functions);
    
    // Performance
    out << "\n" << colors::bold() << "=== Performance Tests ===" 
        << colors::reset() << std::endl;
    runner.run_test("Large Data", test_large_data);
    runner.run_test("Deeply Nested", test_deeply_nested);
    
    // SuperGrok Enhancements
    out << "\n" << colors::bold() << "=== SuperGrok Enhancement Tests ===" 
        << colors::reset() << std::endl;
    runner.run_test("Depth Limit (Parse)", test_depth_limit_parse);
    runner.run_test("Depth Limit (Dump)", test_depth_limit_dump);
    runner.run_test("Range Checks", test_range_checks);
    runner.run_test("Better Error Messages", test_better_error_messages);
    runner.run_test("Param Helpers", test_param_helpers);
    runner.run_test("Position in Errors", test_position_in_errors);
    
    TEST_SUITE_END();
    
    int failed = runner.print_summary();
    return failed == 0;
}

} // namespace cpp_utilities::testing
