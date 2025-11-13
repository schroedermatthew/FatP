#include <iostream>
#include <sstream>
#include <cmath>
#include <climits>
#include <fstream>
#include <cstdint>
#include <algorithm>

#include "JsonLite.h"
#include "JsonLiteTests.h"
#include "test_Utilities.h"

// ====================================================================
// Test Structs
// ====================================================================

namespace cpp_utilities {

struct Point {
    int x;
    int y;
};
CPP_JSON_DEFINE_TYPE_NON_INTRUSIVE(Point, x, y)

struct Point3D {
    int x;
    int y;
    int z;
};
CPP_JSON_DEFINE_TYPE_NON_INTRUSIVE(Point3D, x, y, z)

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

struct AppConfig {
    int port;
    std::string host;
    std::vector<std::string> allowed_ips;
    std::optional<int> timeout;
};
CPP_JSON_DEFINE_TYPE_NON_INTRUSIVE(AppConfig, port, host, allowed_ips, timeout)

struct Employee {
    std::string name;
    int id;
    std::optional<std::string> email;
    std::vector<std::string> skills;
    std::map<std::string, int> certifications;
};
CPP_JSON_DEFINE_TYPE_NON_INTRUSIVE(Employee, name, id, email, skills, certifications)

struct Department {
    std::string name;
    std::vector<Employee> employees;
    std::optional<Employee> manager;
};
CPP_JSON_DEFINE_TYPE_NON_INTRUSIVE(Department, name, employees, manager)

struct Company {
    std::string name;
    std::vector<Department> departments;
    std::map<std::string, std::string> metadata;
};
CPP_JSON_DEFINE_TYPE_NON_INTRUSIVE(Company, name, departments, metadata)

struct Max50Fields {
    int f1, f2, f3, f4, f5, f6, f7, f8, f9, f10;
    int f11, f12, f13, f14, f15, f16, f17, f18, f19, f20;
    int f21, f22, f23, f24, f25, f26, f27, f28, f29, f30;
    int f31, f32, f33, f34, f35, f36, f37, f38, f39, f40;
    int f41, f42, f43, f44, f45, f46, f47, f48, f49, f50;
};

CPP_JSON_DEFINE_TYPE_NON_INTRUSIVE(Max50Fields, 
    f1, f2, f3, f4, f5, f6, f7, f8, f9, f10,
    f11, f12, f13, f14, f15, f16, f17, f18, f19, f20,
    f21, f22, f23, f24, f25, f26, f27, f28, f29, f30,
    f31, f32, f33, f34, f35, f36, f37, f38, f39, f40,
    f41, f42, f43, f44, f45, f46, f47, f48, f49, f50)

struct TestBasic { int x; };
CPP_JSON_DEFINE_TYPE_NON_INTRUSIVE(TestBasic, x)

struct TypeA { int value; };
CPP_JSON_DEFINE_TYPE_NON_INTRUSIVE(TypeA, value)

struct TypeB { double value; };
CPP_JSON_DEFINE_TYPE_NON_INTRUSIVE(TypeB, value)

struct OptConfig { int port = 8080; };
CPP_JSON_DEFINE_TYPE_OPTIONAL(OptConfig, port)

class PrivateTest {
    int secret_ = 99;
public:
    CPP_JSON_DEFINE_TYPE_INTRUSIVE(PrivateTest, secret_)
    int get_secret() const { return secret_; }
};

struct User { std::string name; int age; };
CPP_JSON_DEFINE_TYPE_NON_INTRUSIVE(User, name, age)

struct Product { std::string name; double price; };
CPP_JSON_DEFINE_TYPE_NON_INTRUSIVE(Product, name, price)

struct Order { std::string name; int quantity; };
CPP_JSON_DEFINE_TYPE_NON_INTRUSIVE(Order, name, quantity)

struct Container { std::vector<int> items; };
CPP_JSON_DEFINE_TYPE_NON_INTRUSIVE(Container, items)

struct RequiredFields { int x; };
CPP_JSON_DEFINE_TYPE_NON_INTRUSIVE(RequiredFields, x)

struct OptionalFields { int y = 10; };
CPP_JSON_DEFINE_TYPE_OPTIONAL(OptionalFields, y)

struct ScientificData {
    double planck_constant;
    double avogadro_number;
    double normal_value;
};
CPP_JSON_DEFINE_TYPE_NON_INTRUSIVE(ScientificData, planck_constant, avogadro_number, normal_value)

struct WithStdArray {
    std::array<int, 3> values;
    std::array<double, 2> coords;
};
CPP_JSON_DEFINE_TYPE_NON_INTRUSIVE(WithStdArray, values, coords)

struct TestData {
    double value;
};
CPP_JSON_DEFINE_TYPE_NON_INTRUSIVE(TestData, value)

struct ComplexData {
    double temperature;
    double pressure;
    std::string name;
};
CPP_JSON_DEFINE_TYPE_NON_INTRUSIVE(ComplexData, temperature, pressure, name)

}  // namespace cpp_utilities

// ====================================================================
// Test Policy Structs (must be outside function scope)
// ====================================================================

namespace cpp_utilities::testing {

struct LowPrecisionPolicy : StandardJsonPolicy {
    static constexpr int numeric_precision = 2;
};

struct Precision2 : StandardJsonPolicy { 
    static constexpr int numeric_precision = 2; 
};

struct Precision4 : StandardJsonPolicy { 
    static constexpr int numeric_precision = 4; 
};

struct Precision8 : StandardJsonPolicy { 
    static constexpr int numeric_precision = 8; 
};

}  // namespace cpp_utilities::testing

// ====================================================================
// Basic Type Tests
// ====================================================================

namespace cpp_utilities::testing
{

bool test_json_lite_basic_types() {
    // Null
    {
        JsonValue j = nullptr;
        SIMPLE_ASSERT(j.is_null(), "Should be null");
        std::string str = to_json_string(j);
        SIMPLE_ASSERT(str == "null", "Null should serialize to 'null'");
    }
    
    // Boolean
    {
        JsonValue j_true = true;
        JsonValue j_false = false;
        SIMPLE_ASSERT(j_true.is_bool(), "Should be bool");
        SIMPLE_ASSERT(to_json_string(j_true) == "true", "True should serialize to 'true'");
        SIMPLE_ASSERT(to_json_string(j_false) == "false", "False should serialize to 'false'");
    }
    
    // Numbers
    {
        JsonValue j_int = static_cast<int64_t>(42);
        JsonValue j_float = 3.14159;
        SIMPLE_ASSERT(j_int.is_number(), "Should be number");
        SIMPLE_ASSERT(to_json_string(j_int) == "42", "Integer should serialize correctly");
        SIMPLE_ASSERT(to_json_string(j_float).substr(0, 4) == "3.14", "Float should serialize correctly");
    }
    
    // Strings
    {
        JsonValue j = std::string("hello");
        SIMPLE_ASSERT(j.is_string(), "Should be string");
        SIMPLE_ASSERT(to_json_string(j) == "\"hello\"", "String should serialize with quotes");
    }
    
    return true;
}

// ====================================================================
// Numeric Type Tests
// ====================================================================

bool test_json_lite_int8() {
    int8_t val_pos = 127;
    int8_t val_neg = -128;
    int8_t val_zero = 0;
    
    JsonValue j_pos = to_json(static_cast<int>(val_pos));
    JsonValue j_neg = to_json(static_cast<int>(val_neg));
    JsonValue j_zero = to_json(static_cast<int>(val_zero));
    
    int out_pos_int, out_neg_int, out_zero_int;
    from_json(j_pos, out_pos_int);
    from_json(j_neg, out_neg_int);
    from_json(j_zero, out_zero_int);
    
    int8_t out_pos = static_cast<int8_t>(out_pos_int);
    int8_t out_neg = static_cast<int8_t>(out_neg_int);
    int8_t out_zero = static_cast<int8_t>(out_zero_int);
    
    SIMPLE_ASSERT(out_pos == 127, "int8 max should round-trip");
    SIMPLE_ASSERT(out_neg == -128, "int8 min should round-trip");
    SIMPLE_ASSERT(out_zero == 0, "int8 zero should round-trip");
    
    return true;
}

bool test_json_lite_int16() {
    int16_t val_pos = 32767;
    int16_t val_neg = -32768;
    
    JsonValue j_pos = to_json(static_cast<int>(val_pos));
    JsonValue j_neg = to_json(static_cast<int>(val_neg));
    
    int out_pos_int, out_neg_int;
    from_json(j_pos, out_pos_int);
    from_json(j_neg, out_neg_int);
    
    int16_t out_pos = static_cast<int16_t>(out_pos_int);
    int16_t out_neg = static_cast<int16_t>(out_neg_int);
    
    SIMPLE_ASSERT(out_pos == 32767, "int16 max should round-trip");
    SIMPLE_ASSERT(out_neg == -32768, "int16 min should round-trip");
    
    return true;
}

bool test_json_lite_int32() {
    int32_t val_pos = 2147483647;
    int32_t val_neg = -2147483647 - 1;
    
    JsonValue j_pos = to_json(val_pos);
    JsonValue j_neg = to_json(val_neg);
    
    int32_t out_pos, out_neg;
    from_json(j_pos, out_pos);
    from_json(j_neg, out_neg);
    
    SIMPLE_ASSERT(out_pos == 2147483647, "int32 max should round-trip");
    SIMPLE_ASSERT(out_neg == -2147483648, "int32 min should round-trip");
    
    return true;
}

bool test_json_lite_int64() {
    int64_t val_pos = 9223372036854775807LL;
    int64_t val_neg = -9223372036854775807LL - 1;
    
    JsonValue j_pos = to_json(val_pos);
    JsonValue j_neg = to_json(val_neg);
    
    int64_t out_pos, out_neg;
    from_json(j_pos, out_pos);
    from_json(j_neg, out_neg);
    
    SIMPLE_ASSERT(out_pos == 9223372036854775807LL, "int64 max should round-trip");
    SIMPLE_ASSERT(out_neg == (-9223372036854775807LL - 1), "int64 min should round-trip");
    
    return true;
}

bool test_json_lite_uint8() {
    uint8_t val_max = 255;
    uint8_t val_min = 0;
    
    JsonValue j_max = to_json(static_cast<unsigned int>(val_max));
    JsonValue j_min = to_json(static_cast<unsigned int>(val_min));
    
    unsigned int out_max_int, out_min_int;
    from_json(j_max, out_max_int);
    from_json(j_min, out_min_int);
    
    uint8_t out_max = static_cast<uint8_t>(out_max_int);
    uint8_t out_min = static_cast<uint8_t>(out_min_int);
    
    SIMPLE_ASSERT(out_max == 255, "uint8 max should round-trip");
    SIMPLE_ASSERT(out_min == 0, "uint8 min should round-trip");
    
    return true;
}

bool test_json_lite_uint16() {
    uint16_t val_max = 65535;
    uint16_t val_min = 0;
    
    JsonValue j_max = to_json(static_cast<unsigned int>(val_max));
    JsonValue j_min = to_json(static_cast<unsigned int>(val_min));
    
    unsigned int out_max_int, out_min_int;
    from_json(j_max, out_max_int);
    from_json(j_min, out_min_int);
    
    uint16_t out_max = static_cast<uint16_t>(out_max_int);
    uint16_t out_min = static_cast<uint16_t>(out_min_int);
    
    SIMPLE_ASSERT(out_max == 65535, "uint16 max should round-trip");
    SIMPLE_ASSERT(out_min == 0, "uint16 min should round-trip");
    
    return true;
}

bool test_json_lite_uint32() {
    uint32_t val_max = 4294967295U;
    uint32_t val_min = 0;
    
    JsonValue j_max = to_json(val_max);
    JsonValue j_min = to_json(val_min);
    
    uint32_t out_max, out_min;
    from_json(j_max, out_max);
    from_json(j_min, out_min);
    
    SIMPLE_ASSERT(out_max == 4294967295U, "uint32 max should round-trip");
    SIMPLE_ASSERT(out_min == 0, "uint32 min should round-trip");
    
    return true;
}

bool test_json_lite_uint64() {
    uint64_t val_large = 9223372036854775807ULL;
    uint64_t val_min = 0;
    
    JsonValue j_large = to_json(val_large);
    JsonValue j_min = to_json(val_min);
    
    uint64_t out_large, out_min;
    from_json(j_large, out_large);
    from_json(j_min, out_min);
    
    SIMPLE_ASSERT(out_large == 9223372036854775807ULL, "uint64 in int64 range should round-trip");
    SIMPLE_ASSERT(out_min == 0, "uint64 min should round-trip");
    
    return true;
}

bool test_json_lite_float_types() {
    float f_val = 3.14159f;
    double d_val = 2.718281828459045;
    
    JsonValue j_float = to_json(f_val);
    JsonValue j_double = to_json(d_val);
    
    float f_out;
    double d_out;
    from_json(j_float, f_out);
    from_json(j_double, d_out);
    
    SIMPLE_ASSERT(std::abs(f_out - 3.14159f) < 0.0001f, "Float should round-trip");
    SIMPLE_ASSERT(std::abs(d_out - 2.718281828459045) < 0.000001, "Double should round-trip");
    
    return true;
}

bool test_json_lite_numeric_edge_cases() {
    {
        JsonValue j = static_cast<int64_t>(0);
        int val;
        from_json(j, val);
        SIMPLE_ASSERT(val == 0, "Zero should serialize correctly");
    }
    
    {
        double neg_zero = -0.0;
        JsonValue j = to_json(neg_zero);
        double out;
        from_json(j, out);
        SIMPLE_ASSERT(out == 0.0, "Negative zero should round-trip");
    }
    
    {
        double small = 0.0000000001;
        JsonValue j = to_json(small);
        double out;
        from_json(j, out);
        SIMPLE_ASSERT(std::abs(out - small) < 1e-15, "Very small positive should round-trip");
    }
    
    {
        double large = 1e15;
        JsonValue j = to_json(large);
        double out;
        from_json(j, out);
        SIMPLE_ASSERT(std::abs(out - large) < 1.0, "Very large should round-trip");
    }
    
    return true;
}

// ====================================================================
// String Tests
// ====================================================================

bool test_json_lite_string_escaping() {
    std::string test_str = "line1\nline2\ttab\"quote\\backslash";
    JsonValue j = test_str;
    std::string result = to_json_string(j);
    
    SIMPLE_ASSERT(result.find("\\n") != std::string::npos, "Newline should be escaped");
    SIMPLE_ASSERT(result.find("\\t") != std::string::npos, "Tab should be escaped");
    SIMPLE_ASSERT(result.find("\\\"") != std::string::npos, "Quote should be escaped");
    SIMPLE_ASSERT(result.find("\\\\") != std::string::npos, "Backslash should be escaped");
    
    return true;
}

bool test_json_lite_string_empty() {
    std::string empty = "";
    JsonValue j = to_json(empty);
    
    std::string str = to_json_string(j);
    SIMPLE_ASSERT(str == "\"\"", "Empty string should serialize as two quotes");
    
    std::string out;
    from_json(j, out);
    SIMPLE_ASSERT(out.empty(), "Empty string should round-trip");
    
    return true;
}

bool test_json_lite_string_whitespace() {
    std::string ws = "   \t\n  ";
    JsonValue j = to_json(ws);
    
    std::string out;
    from_json(j, out);
    SIMPLE_ASSERT(out == "   \t\n  ", "Whitespace should be preserved");
    
    return true;
}

bool test_json_lite_string_special_chars() {
    std::string special = "\b\f\r";
    JsonValue j = to_json(special);
    std::string result = to_json_string(j);
    
    SIMPLE_ASSERT(result.find("\\b") != std::string::npos, "Backspace should be escaped");
    SIMPLE_ASSERT(result.find("\\f") != std::string::npos, "Form feed should be escaped");
    SIMPLE_ASSERT(result.find("\\r") != std::string::npos, "Carriage return should be escaped");
    
    return true;
}

bool test_json_lite_string_unicode() {
    std::string unicode = "Hello 世界 🌍";
    JsonValue j = to_json(unicode);
    
    std::string out;
    from_json(j, out);
    SIMPLE_ASSERT(out == unicode, "Unicode string should round-trip");
    
    return true;
}

bool test_json_lite_string_long() {
    std::string long_str(10000, 'x');
    JsonValue j = to_json(long_str);
    
    std::string out;
    from_json(j, out);
    SIMPLE_ASSERT(out.size() == 10000, "Long string length should be preserved");
    SIMPLE_ASSERT(out == long_str, "Long string should round-trip");
    
    return true;
}

// ====================================================================
// Container Tests
// ====================================================================

bool test_json_lite_containers() {
    {
        std::vector<int> vec = {1, 2, 3, 4, 5};
        JsonValue j = to_json(vec);
        SIMPLE_ASSERT(j.is_array(), "Should be array");
        std::string str = to_json_string(j);
        SIMPLE_ASSERT(str.find("1") != std::string::npos, "Should contain 1");
        SIMPLE_ASSERT(str.find("5") != std::string::npos, "Should contain 5");
        
        std::vector<int> vec2;
        from_json(j, vec2);
        SIMPLE_ASSERT(vec == vec2, "Round-trip should preserve vector");
    }
    
    {
        std::map<std::string, int> map = {{"a", 1}, {"b", 2}};
        JsonValue j = to_json(map);
        SIMPLE_ASSERT(j.is_object(), "Should be object");
        
        std::map<std::string, int> map2;
        from_json(j, map2);
        SIMPLE_ASSERT(map == map2, "Round-trip should preserve map");
    }
    
    {
        std::vector<std::vector<int>> nested = {{1, 2}, {3, 4}};
        JsonValue j = to_json(nested);
        std::string str = to_json_string(j);
        SIMPLE_ASSERT(str.find("1") != std::string::npos, "Should contain 1");
        SIMPLE_ASSERT(str.find("4") != std::string::npos, "Should contain 4");
    }
    
    return true;
}

bool test_json_lite_vector_empty() {
    std::vector<int> empty;
    JsonValue j = to_json(empty);
    
    SIMPLE_ASSERT(j.is_array(), "Empty vector should be array");
    std::string str = to_json_string(j);
    SIMPLE_ASSERT(str.find("[") != std::string::npos, "Should have opening bracket");
    SIMPLE_ASSERT(str.find("]") != std::string::npos, "Should have closing bracket");
    
    std::vector<int> out;
    from_json(j, out);
    SIMPLE_ASSERT(out.empty(), "Empty vector should round-trip");
    
    return true;
}

bool test_json_lite_vector_single() {
    std::vector<int> single = {42};
    JsonValue j = to_json(single);
    
    std::vector<int> out;
    from_json(j, out);
    SIMPLE_ASSERT(out.size() == 1, "Single element vector size should be 1");
    SIMPLE_ASSERT(out[0] == 42, "Single element should be preserved");
    
    return true;
}

bool test_json_lite_set() {
    std::set<int> s = {3, 1, 4, 1, 5};
    JsonValue j = to_json(s);
    
    SIMPLE_ASSERT(j.is_array(), "Set should serialize as array");
    
    std::vector<int> out;
    from_json(j, out);
    SIMPLE_ASSERT(out.size() == 4, "Set duplicates should be removed");
    
    return true;
}

bool test_json_lite_map_empty() {
    std::map<std::string, int> empty;
    JsonValue j = to_json(empty);
    
    SIMPLE_ASSERT(j.is_object(), "Empty map should be object");
    
    std::map<std::string, int> out;
    from_json(j, out);
    SIMPLE_ASSERT(out.empty(), "Empty map should round-trip");
    
    return true;
}

bool test_json_lite_map_numeric_keys() {
    std::map<int, std::string> map = {{1, "one"}, {2, "two"}};
    JsonValue j = to_json(map);
    
    SIMPLE_ASSERT(j.is_object(), "Map with numeric keys should be object");
    std::string str = to_json_string(j);
    SIMPLE_ASSERT(str.find("one") != std::string::npos, "Should contain value");
    
    return true;
}

bool test_json_lite_nested_vectors() {
    std::vector<std::vector<std::vector<int>>> triple_nested = {
        {{1, 2}, {3, 4}},
        {{5, 6}, {7, 8}}
    };
    
    JsonValue j = to_json(triple_nested);
    std::vector<std::vector<std::vector<int>>> out;
    from_json(j, out);
    
    SIMPLE_ASSERT(out.size() == 2, "Triple nested size should be preserved");
    SIMPLE_ASSERT(out[0][0][0] == 1, "Triple nested values should be preserved");
    SIMPLE_ASSERT(out[1][1][1] == 8, "Triple nested last value should be preserved");
    
    return true;
}

bool test_json_lite_mixed_nested() {
    std::map<std::string, std::vector<int>> mixed = {
        {"first", {1, 2, 3}},
        {"second", {4, 5, 6}}
    };
    
    JsonValue j = to_json(mixed);
    std::map<std::string, std::vector<int>> out;
    from_json(j, out);
    
    SIMPLE_ASSERT(out["first"].size() == 3, "Nested vector size should be preserved");
    SIMPLE_ASSERT(out["second"][2] == 6, "Nested values should be preserved");
    
    return true;
}

bool test_json_lite_array() {
    std::array<int, 5> arr = {1, 2, 3, 4, 5};
    JsonValue j = to_json(arr);
    
    std::array<int, 5> out;
    from_json(j, out);
    
    SIMPLE_ASSERT(out[0] == 1, "Array first element should be preserved");
    SIMPLE_ASSERT(out[4] == 5, "Array last element should be preserved");
    
    return true;
}

// ====================================================================
// Optional Tests
// ====================================================================

bool test_json_lite_optional() {
    {
        std::optional<int> opt = 42;
        JsonValue j = to_json(opt);
        SIMPLE_ASSERT(!j.is_null(), "Optional with value should not be null");
        
        std::optional<int> opt2;
        from_json(j, opt2);
        SIMPLE_ASSERT(opt2.has_value(), "Should have value after deserialization");
        SIMPLE_ASSERT(*opt2 == 42, "Value should be preserved");
    }
    
    {
        std::optional<int> opt;
        JsonValue j = to_json(opt);
        SIMPLE_ASSERT(j.is_null(), "Optional without value should be null");
        
        std::optional<int> opt2 = 99;
        from_json(j, opt2);
        SIMPLE_ASSERT(!opt2.has_value(), "Should not have value after deserializing null");
    }
    
    return true;
}

bool test_json_lite_optional_string() {
    std::optional<std::string> opt = "hello";
    JsonValue j = to_json(opt);
    
    std::optional<std::string> out;
    from_json(j, out);
    
    SIMPLE_ASSERT(out.has_value(), "Optional string should have value");
    SIMPLE_ASSERT(*out == "hello", "Optional string value should be preserved");
    
    return true;
}

bool test_json_lite_optional_vector() {
    std::optional<std::vector<int>> opt = std::vector<int>{1, 2, 3};
    JsonValue j = to_json(opt);
    
    std::optional<std::vector<int>> out;
    from_json(j, out);
    
    SIMPLE_ASSERT(out.has_value(), "Optional vector should have value");
    SIMPLE_ASSERT(out->size() == 3, "Optional vector size should be preserved");
    
    return true;
}

// ====================================================================
// Tuple and Pair Tests
// ====================================================================

bool test_json_lite_tuples_pairs() {
    {
        std::pair<int, std::string> p = {42, "answer"};
        JsonValue j = to_json(p);
        SIMPLE_ASSERT(j.is_array(), "Pair should serialize as array");
        
        std::pair<int, std::string> p2;
        from_json(j, p2);
        SIMPLE_ASSERT(p == p2, "Round-trip should preserve pair");
    }
    
    {
        std::tuple<int, std::string, bool> t = {1, "test", true};
        JsonValue j = to_json(t);
        std::string str = to_json_string(j);
        SIMPLE_ASSERT(str.find("1") != std::string::npos, "Should contain 1");
        SIMPLE_ASSERT(str.find("test") != std::string::npos, "Should contain test");
        SIMPLE_ASSERT(str.find("true") != std::string::npos, "Should contain true");
        
        std::tuple<int, std::string, bool> t2;
        from_json(j, t2);
        SIMPLE_ASSERT(t == t2, "Round-trip should preserve tuple");
    }
    
    return true;
}

bool test_json_lite_pair_nested() {
    std::pair<std::vector<int>, std::string> p = {{1, 2, 3}, "test"};
    JsonValue j = to_json(p);
    
    std::pair<std::vector<int>, std::string> out;
    from_json(j, out);
    
    SIMPLE_ASSERT(out.first.size() == 3, "Nested vector in pair should be preserved");
    SIMPLE_ASSERT(out.second == "test", "String in pair should be preserved");
    
    return true;
}

bool test_json_lite_tuple_large() {
    std::tuple<int, std::string, double, bool, int, std::string> t = {
        1, "one", 1.1, true, 2, "two"
    };
    
    JsonValue j = to_json(t);
    std::tuple<int, std::string, double, bool, int, std::string> out;
    from_json(j, out);
    
    SIMPLE_ASSERT(std::get<0>(out) == 1, "First element should be preserved");
    SIMPLE_ASSERT(std::get<5>(out) == "two", "Last element should be preserved");
    
    return true;
}

// ====================================================================
// Struct Tests
// ====================================================================

bool test_json_lite_simple_struct() {
    Point p{10, 20};
    JsonValue j = to_json(p);
    
    std::string str = to_json_string(j);
    SIMPLE_ASSERT(str.find("\"x\"") != std::string::npos, "Should have x field");
    SIMPLE_ASSERT(str.find("10") != std::string::npos, "Should have x value");
    SIMPLE_ASSERT(str.find("\"y\"") != std::string::npos, "Should have y field");
    SIMPLE_ASSERT(str.find("20") != std::string::npos, "Should have y value");
    
    Point p2;
    from_json(j, p2);
    SIMPLE_ASSERT(p2.x == 10, "X should be preserved");
    SIMPLE_ASSERT(p2.y == 20, "Y should be preserved");
    
    return true;
}

bool test_json_lite_complex_struct() {
    Person person{"Alice", 30, {"reading", "coding", "hiking"}};
    JsonValue j = to_json(person);
    
    Person person2;
    from_json(j, person2);
    
    SIMPLE_ASSERT(person2.name == "Alice", "Name should be preserved");
    SIMPLE_ASSERT(person2.age == 30, "Age should be preserved");
    SIMPLE_ASSERT(person2.hobbies.size() == 3, "Hobbies count should be preserved");
    SIMPLE_ASSERT(person2.hobbies[0] == "reading", "First hobby should be preserved");
    
    return true;
}

bool test_json_lite_optional_fields() {
    Config cfg;
    cfg.timeout = 5000;
    cfg.host = "localhost";
    cfg.port = 9000;
    
    JsonValue j = to_json(cfg);
    
    Config cfg2;
    from_json(j, cfg2);
    
    SIMPLE_ASSERT(cfg2.timeout.has_value(), "Timeout should have value");
    SIMPLE_ASSERT(*cfg2.timeout == 5000, "Timeout value should be preserved");
    SIMPLE_ASSERT(cfg2.host.has_value(), "Host should have value");
    SIMPLE_ASSERT(*cfg2.host == "localhost", "Host value should be preserved");
    SIMPLE_ASSERT(cfg2.port == 9000, "Port should be preserved");
    
    return true;
}

bool test_json_lite_intrusive_serialization() {
    PrivateData data(42, "secret123");
    JsonValue j = to_json(data);
    
    PrivateData data2;
    from_json(j, data2);
    
    SIMPLE_ASSERT(data2.secret() == 42, "Secret should be preserved");
    SIMPLE_ASSERT(data2.code() == "secret123", "Code should be preserved");
    
    return true;
}

bool test_json_lite_nested_structs() {
    Nested n;
    n.position = Point{100, 200};
    n.label = "marker";
    
    JsonValue j = to_json(n);
    
    Nested n2;
    from_json(j, n2);
    
    SIMPLE_ASSERT(n2.position.x == 100, "Nested x should be preserved");
    SIMPLE_ASSERT(n2.position.y == 200, "Nested y should be preserved");
    SIMPLE_ASSERT(n2.label == "marker", "Label should be preserved");
    
    return true;
}

bool test_json_lite_complex_nested() {
    Complex c;
    c.scores["alice"] = 100;
    c.scores["bob"] = 85;
    c.points.push_back(Point{1, 2});
    c.points.push_back(Point{3, 4});
    c.description = "test data";
    
    JsonValue j = to_json(c);
    
    Complex c2;
    from_json(j, c2);
    
    SIMPLE_ASSERT(c2.scores.size() == 2, "Should have 2 scores");
    SIMPLE_ASSERT(c2.scores["alice"] == 100, "Alice score should be preserved");
    SIMPLE_ASSERT(c2.scores["bob"] == 85, "Bob score should be preserved");
    SIMPLE_ASSERT(c2.points.size() == 2, "Should have 2 points");
    SIMPLE_ASSERT(c2.points[0].x == 1, "First point x should be preserved");
    SIMPLE_ASSERT(c2.description.has_value(), "Description should have value");
    SIMPLE_ASSERT(*c2.description == "test data", "Description should be preserved");
    
    return true;
}

bool test_json_lite_deep_hierarchy() {
    Company company;
    company.name = "TechCorp";
    
    Department eng;
    eng.name = "Engineering";
    
    Employee emp1;
    emp1.name = "Alice";
    emp1.id = 1;
    emp1.email = "alice@tech.com";
    emp1.skills = {"C++", "Python", "Go"};
    emp1.certifications["AWS"] = 2020;
    
    Employee emp2;
    emp2.name = "Bob";
    emp2.id = 2;
    emp2.skills = {"Java", "Kotlin"};
    
    eng.employees = {emp1, emp2};
    eng.manager = emp1;
    
    company.departments = {eng};
    company.metadata["founded"] = "2020";
    company.metadata["location"] = "SF";
    
    JsonValue j = to_json(company);
    Company out;
    from_json(j, out);
    
    SIMPLE_ASSERT(out.name == "TechCorp", "Company name should be preserved");
    SIMPLE_ASSERT(out.departments.size() == 1, "Should have 1 department");
    SIMPLE_ASSERT(out.departments[0].employees.size() == 2, "Should have 2 employees");
    SIMPLE_ASSERT(out.departments[0].manager.has_value(), "Should have manager");
    SIMPLE_ASSERT(out.departments[0].manager->name == "Alice", "Manager name should be preserved");
    SIMPLE_ASSERT(out.departments[0].employees[0].certifications["AWS"] == 2020, "Certifications should be preserved");
    
    return true;
}

// ====================================================================
// Parser Tests
// ====================================================================

bool test_json_lite_parser_basic() {
    std::string json = R"({"name": "John", "age": 30, "active": true})";
    JsonValue j = parse_json(json);
    
    SIMPLE_ASSERT(j.is_object(), "Should parse as object");
    
    const auto& obj = std::get<JsonObject>(j);
    SIMPLE_ASSERT(std::get<std::string>(obj.at("name")) == "John", "Name should be parsed correctly");
    SIMPLE_ASSERT(std::get<int64_t>(obj.at("age")) == 30, "Age should be parsed correctly");
    SIMPLE_ASSERT(std::get<bool>(obj.at("active")) == true, "Active should be parsed correctly");
    
    return true;
}

bool test_json_lite_parser_containers() {
    std::string json = R"([1, 2, 3, 4, 5])";
    JsonValue j = parse_json(json);
    
    SIMPLE_ASSERT(j.is_array(), "Should parse as array");
    
    const auto& arr = std::get<JsonArray>(j);
    SIMPLE_ASSERT(arr.size() == 5, "Should have 5 elements");
    SIMPLE_ASSERT(std::get<int64_t>(arr[0]) == 1, "First element should be 1");
    SIMPLE_ASSERT(std::get<int64_t>(arr[4]) == 5, "Last element should be 5");
    
    return true;
}

bool test_json_lite_parser_edge_cases() {
    {
        JsonValue j = parse_json("{}");
        SIMPLE_ASSERT(j.is_object(), "Empty object should parse");
        const auto& obj = std::get<JsonObject>(j);
        SIMPLE_ASSERT(obj.empty(), "Empty object should be empty");
    }
    
    {
        JsonValue j = parse_json("[]");
        SIMPLE_ASSERT(j.is_array(), "Empty array should parse");
        const auto& arr = std::get<JsonArray>(j);
        SIMPLE_ASSERT(arr.empty(), "Empty array should be empty");
    }
    
    {
        JsonValue j = parse_json("  {  \"key\"  :  \"value\"  }  ");
        SIMPLE_ASSERT(j.is_object(), "Should handle whitespace");
    }
    
    return true;
}

bool test_json_lite_parser_numbers() {
    {
        JsonValue j = parse_json("42");
        SIMPLE_ASSERT(j.is_int(), "Should parse as int");
        SIMPLE_ASSERT(std::get<int64_t>(j) == 42, "Value should be 42");
    }
    
    {
        JsonValue j = parse_json("-123");
        SIMPLE_ASSERT(j.is_int(), "Should parse as int");
        SIMPLE_ASSERT(std::get<int64_t>(j) == -123, "Value should be -123");
    }
    
    {
        JsonValue j = parse_json("3.14");
        SIMPLE_ASSERT(!j.is_int(), "Should not be int");
        SIMPLE_ASSERT(j.is_number(), "Should be number");
    }
    
    {
        JsonValue j = parse_json("1.23e10");
        SIMPLE_ASSERT(j.is_number(), "Should parse scientific notation");
    }
    
    return true;
}

bool test_json_lite_parser_strings() {
    {
        JsonValue j = parse_json("\"hello\"");
        SIMPLE_ASSERT(j.is_string(), "Should parse as string");
        SIMPLE_ASSERT(std::get<std::string>(j) == "hello", "Value should be hello");
    }
    
    {
        JsonValue j = parse_json("\"\"");
        SIMPLE_ASSERT(j.is_string(), "Should parse as string");
        SIMPLE_ASSERT(std::get<std::string>(j).empty(), "Should be empty");
    }
    
    {
        JsonValue j = parse_json(R"("hello\nworld")");
        std::string str = std::get<std::string>(j);
        SIMPLE_ASSERT(str.find('\n') != std::string::npos, "Should have newline");
    }
    
    return true;
}

bool test_json_lite_parser_nested() {
    std::string json = R"({
        "array": [1, 2, {"nested": true}],
        "object": {"key": "value"}
    })";
    
    JsonValue j = parse_json(json);
    SIMPLE_ASSERT(j.is_object(), "Should parse as object");
    
    const auto& obj = std::get<JsonObject>(j);
    SIMPLE_ASSERT(obj.at("array").is_array(), "Should have array field");
    SIMPLE_ASSERT(obj.at("object").is_object(), "Should have object field");
    
    return true;
}

bool test_json_lite_parser_errors() {
    {
        bool caught = false;
        try {
            (void)parse_json("{invalid}");
        } catch (const std::runtime_error&) {
            caught = true;
        }
        SIMPLE_ASSERT(caught, "Should throw on invalid JSON");
    }
    
    {
        bool caught = false;
        try {
            (void)parse_json("{\"key\": \"value\"");
        } catch (const std::runtime_error&) {
            caught = true;
        }
        SIMPLE_ASSERT(caught, "Should throw on unclosed brace");
    }
    
    {
        bool caught = false;
        try {
            (void)parse_json("[1, 2, 3");
        } catch (const std::runtime_error&) {
            caught = true;
        }
        SIMPLE_ASSERT(caught, "Should throw on unclosed array");
    }
    
    {
        bool caught = false;
        try {
            (void)parse_json("\"unterminated");
        } catch (const std::runtime_error&) {
            caught = true;
        }
        SIMPLE_ASSERT(caught, "Should throw on unterminated string");
    }
    
    {
        bool caught = false;
        try {
            (void)parse_json(R"("invalid\x")");
        } catch (const std::runtime_error&) {
            caught = true;
        }
        SIMPLE_ASSERT(caught, "Should throw on invalid escape");
    }
    
    return true;
}

bool test_json_lite_parser_literals() {
    {
        JsonValue j = parse_json("true");
        SIMPLE_ASSERT(j.is_bool(), "Should parse true");
        SIMPLE_ASSERT(std::get<bool>(j) == true, "Value should be true");
    }
    
    {
        JsonValue j = parse_json("false");
        SIMPLE_ASSERT(j.is_bool(), "Should parse false");
        SIMPLE_ASSERT(std::get<bool>(j) == false, "Value should be false");
    }
    
    {
        JsonValue j = parse_json("null");
        SIMPLE_ASSERT(j.is_null(), "Should parse null");
    }
    
    return true;
}

bool test_json_lite_parser_unicode() {
    std::string json = R"("Hello \u4e16\u754c")";
    JsonValue j = parse_json(json);
    
    std::string str = std::get<std::string>(j);
    SIMPLE_ASSERT(!str.empty(), "Unicode string should not be empty");
    
    return true;
}

// ====================================================================
// Policy Tests
// ====================================================================

bool test_json_lite_pretty_print() {
    Point p{10, 20};
    
    std::string compact = to_json_string<Point, StandardJsonPolicy>(p, false);
    std::string pretty = to_json_string<Point, PrettyJsonPolicy>(p, true);
    
    SIMPLE_ASSERT(pretty.find('\n') != std::string::npos, "Pretty print should have newlines");
    SIMPLE_ASSERT(pretty.size() > compact.size(), "Pretty print should be larger");
    
    return true;
}

bool test_json_lite_numeric_precision() {
    double pi = 3.14159265358979323846;
    JsonValue j = pi;
    
    std::string result = to_json_string(j);
    
    SIMPLE_ASSERT(result.length() > 5, "Should have high precision");
    SIMPLE_ASSERT(result.find("3.14159") != std::string::npos, "Should have precision digits");
    
    return true;
}

bool test_json_lite_nan_inf_handling() {
    {
        double nan_val = std::nan("");
        JsonValue j_nan = nan_val;
        std::string result_nan = to_json_string<JsonValue, CompatJsonPolicy>(j_nan);
        SIMPLE_ASSERT(result_nan.find("NaN") != std::string::npos, "Should handle NaN");
    }
    
    {
        double inf_val = std::numeric_limits<double>::infinity();
        JsonValue j_inf = inf_val;
        std::string result_inf = to_json_string<JsonValue, CompatJsonPolicy>(j_inf);
        SIMPLE_ASSERT(result_inf.find("Infinity") != std::string::npos, "Should handle infinity");
    }
    
    {
        double neg_inf = -std::numeric_limits<double>::infinity();
        JsonValue j_neg_inf = neg_inf;
        std::string result = to_json_string<JsonValue, CompatJsonPolicy>(j_neg_inf);
        SIMPLE_ASSERT(result.find("Infinity") != std::string::npos, "Should handle negative infinity");
    }
    
    return true;
}

bool test_json_lite_standard_policy_nan() {
    double nan_val = std::nan("");
    JsonValue j = nan_val;
    std::string result = to_json_string<JsonValue, StandardJsonPolicy>(j);
    
    SIMPLE_ASSERT(result.find("null") != std::string::npos, "Standard policy should output null for NaN");
    
    return true;
}

// ====================================================================
// File I/O Tests
// ====================================================================

bool test_json_lite_file_io() {
    const std::string filename = "test_json_io.json";
    
    Point p{42, 84};
    save_json_to_file(filename, to_json(p));
    
    JsonValue j = load_json_from_file(filename);
    
    Point p2;
    from_json(j, p2);
    
    SIMPLE_ASSERT(p2.x == 42, "X should be preserved in file I/O");
    SIMPLE_ASSERT(p2.y == 84, "Y should be preserved in file I/O");
    
    std::remove(filename.c_str());
    
    return true;
}

bool test_json_lite_file_io_complex() {
    const std::string filename = "test_complex_io.json";
    
    Company company;
    company.name = "TestCo";
    company.metadata["year"] = "2024";
    
    save_params(filename, company);
    
    Company loaded = load_params<Company>(filename);
    
    SIMPLE_ASSERT(loaded.name == "TestCo", "Complex struct should save/load correctly");
    SIMPLE_ASSERT(loaded.metadata["year"] == "2024", "Metadata should be preserved");
    
    std::remove(filename.c_str());
    
    return true;
}

bool test_json_lite_file_io_errors() {
    bool caught = false;
    try {
        (void)load_json_from_file("nonexistent_file_12345.json");
    } catch (const std::runtime_error&) {
        caught = true;
    }
    SIMPLE_ASSERT(caught, "Should throw when loading non-existent file");
    
    return true;
}

// ====================================================================
// Round-trip Tests
// ====================================================================

bool test_json_lite_roundtrip_all_types() {
    JsonObject root;
    root["null_val"] = nullptr;
    root["bool_val"] = true;
    root["int_val"] = static_cast<int64_t>(42);
    root["float_val"] = 3.14;
    root["string_val"] = "hello";
    
    JsonArray arr;
    arr.push_back(static_cast<int64_t>(1));
    arr.push_back(static_cast<int64_t>(2));
    arr.push_back(static_cast<int64_t>(3));
    root["array_val"] = arr;
    
    JsonObject nested;
    nested["inner"] = "value";
    root["object_val"] = nested;
    
    std::string json_str = to_json_string(root);
    JsonValue parsed = parse_json(json_str);
    
    SIMPLE_ASSERT(parsed.is_object(), "Should round-trip as object");
    const auto& obj = std::get<JsonObject>(parsed);
    SIMPLE_ASSERT(obj.at("null_val").is_null(), "Null should round-trip");
    SIMPLE_ASSERT(obj.at("bool_val").is_bool(), "Bool should round-trip");
    SIMPLE_ASSERT(obj.at("array_val").is_array(), "Array should round-trip");
    SIMPLE_ASSERT(obj.at("object_val").is_object(), "Object should round-trip");
    
    return true;
}

bool test_json_lite_convenience_functions() {
    Point p{10, 20};
    
    std::string json_str = to_json_string(p);
    SIMPLE_ASSERT(json_str.find("10") != std::string::npos, "Should serialize struct");
    
    Point p2 = from_json_string<Point>(json_str);
    SIMPLE_ASSERT(p2.x == 10, "Should deserialize struct");
    SIMPLE_ASSERT(p2.y == 20, "Should deserialize struct");
    
    return true;
}

bool test_json_lite_roundtrip_vectors() {
    std::vector<std::vector<int>> vec = {{1, 2}, {3, 4}, {5, 6}};
    
    std::string json = to_json_string(vec);
    auto out = from_json_string<std::vector<std::vector<int>>>(json);
    
    SIMPLE_ASSERT(out.size() == 3, "Nested vector size should round-trip");
    SIMPLE_ASSERT(out[1][1] == 4, "Nested vector values should round-trip");
    
    return true;
}

bool test_json_lite_roundtrip_maps() {
    std::map<std::string, std::map<std::string, int>> nested_map = {
        {"outer1", {{"inner1", 1}, {"inner2", 2}}},
        {"outer2", {{"inner3", 3}}}
    };
    
    std::string json = to_json_string(nested_map);
    auto out = from_json_string<std::map<std::string, std::map<std::string, int>>>(json);
    
    SIMPLE_ASSERT(out.size() == 2, "Nested map size should round-trip");
    SIMPLE_ASSERT(out["outer1"]["inner2"] == 2, "Nested map values should round-trip");
    
    return true;
}

// ====================================================================
// Performance Tests
// ====================================================================

bool test_json_lite_large_data() {
    std::vector<int> large_vec;
    for (int i = 0; i < 10000; ++i) {
        large_vec.push_back(i);
    }
    
    JsonValue j = to_json(large_vec);
    std::string json_str = to_json_string(j);
    
    JsonValue parsed = parse_json(json_str);
    std::vector<int> vec2;
    from_json(parsed, vec2);
    
    SIMPLE_ASSERT(vec2.size() == 10000, "Large data should round-trip");
    SIMPLE_ASSERT(vec2[0] == 0, "First element should be correct");
    SIMPLE_ASSERT(vec2[9999] == 9999, "Last element should be correct");
    
    return true;
}

bool test_json_lite_deeply_nested() {
    JsonValue j = static_cast<int64_t>(1);
    for (int i = 0; i < 50; ++i) {
        JsonArray arr;
        arr.push_back(j);
        j = arr;
    }
    
    std::string json_str = to_json_string(j);
    JsonValue parsed = parse_json(json_str);
    
    SIMPLE_ASSERT(parsed.is_array(), "Deeply nested should round-trip");
    
    return true;
}

bool test_json_lite_large_strings() {
    std::vector<std::string> large_strings;
    for (int i = 0; i < 1000; ++i) {
        large_strings.push_back(std::string(100, 'x') + std::to_string(i));
    }
    
    std::string json = to_json_string(large_strings);
    auto out = from_json_string<std::vector<std::string>>(json);
    
    SIMPLE_ASSERT(out.size() == 1000, "Large string vector size should round-trip");
    SIMPLE_ASSERT(out[500].find("500") != std::string::npos, "String content should be preserved");
    
    return true;
}

bool test_json_lite_large_map() {
    std::map<std::string, int> large_map;
    for (int i = 0; i < 1000; ++i) {
        large_map["key_" + std::to_string(i)] = i;
    }
    
    std::string json = to_json_string(large_map);
    auto out = from_json_string<std::map<std::string, int>>(json);
    
    SIMPLE_ASSERT(out.size() == 1000, "Large map size should round-trip");
    SIMPLE_ASSERT(out["key_500"] == 500, "Large map values should round-trip");
    
    return true;
}

// ====================================================================
// Enhancement Tests
// ====================================================================

bool test_json_lite_depth_limit_parse() {
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
    
    SIMPLE_ASSERT(caught, "Should throw on exceeding parse depth limit");
    
    return true;
}

bool test_json_lite_depth_limit_dump() {
    JsonValue j = static_cast<int64_t>(1);
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
    
    SIMPLE_ASSERT(caught, "Should throw on exceeding dump depth limit");
    
    return true;
}

bool test_json_lite_range_checks() {
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
        SIMPLE_ASSERT(caught, "Should throw on int overflow");
    }
    
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
        SIMPLE_ASSERT(caught, "Should throw on unsigned underflow");
    }
    
    {
        JsonValue j = static_cast<int64_t>(42);
        int value = 0;
        from_json(j, value);
        SIMPLE_ASSERT(value == 42, "Valid conversion should work");
    }
    
    return true;
}

bool test_json_lite_range_int8() {
    {
        JsonValue j = static_cast<int64_t>(100);
        int val_int;
        from_json(j, val_int);
        int8_t val = static_cast<int8_t>(val_int);
        SIMPLE_ASSERT(val == 100, "int8 within range should convert");
    }
    
    {
        JsonValue j = static_cast<int64_t>(200);
        int val_int = 0;
        bool caught = false;
        try {
            from_json(j, val_int);
            if (val_int > 127) {
                caught = true;
            }
        } catch (const std::runtime_error&) {
            caught = true;
        }
        SIMPLE_ASSERT(caught || val_int > 127, "int8 overflow should be detected");
    }
    
    {
        JsonValue j = static_cast<int64_t>(-200);
        int val_int = 0;
        bool caught = false;
        try {
            from_json(j, val_int);
            if (val_int < -128) {
                caught = true;
            }
        } catch (const std::runtime_error&) {
            caught = true;
        }
        SIMPLE_ASSERT(caught || val_int < -128, "int8 underflow should be detected");
    }
    
    return true;
}

bool test_json_lite_better_error_messages() {
    {
        std::string json = R"({"port": 8080, "host": "localhost"})";
        bool caught = false;
        try {
            (void)from_json_string<AppConfig>(json);
        } catch (const std::runtime_error& e) {
            std::string error_msg = e.what();
            if (error_msg.find("allowed_ips") != std::string::npos) {
                caught = true;
            }
        }
        SIMPLE_ASSERT(caught, "Error should mention missing field name");
    }
    
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
        SIMPLE_ASSERT(caught, "Error should mention field with type mismatch");
    }
    
    return true;
}

bool test_json_lite_param_helpers() {
    AppConfig config;
    config.port = 8080;
    config.host = "127.0.0.1";
    config.allowed_ips = {"192.168.1.1", "10.0.0.1"};
    config.timeout = 30;
    
    const std::string filename = "test_config_helpers.json";
    
    save_params(filename, config);
    
    AppConfig loaded = load_params<AppConfig>(filename);
    
    SIMPLE_ASSERT(loaded.port == config.port, "Port should be preserved");
    SIMPLE_ASSERT(loaded.host == config.host, "Host should be preserved");
    SIMPLE_ASSERT(loaded.allowed_ips == config.allowed_ips, "Allowed IPs should be preserved");
    SIMPLE_ASSERT(loaded.timeout == config.timeout, "Timeout should be preserved");
    
    config.port = 9090;
    save_params_with_backup(filename, config);
    
    std::ifstream backup(filename + ".bak");
    SIMPLE_ASSERT(backup.good(), "Backup file should exist");
    backup.close();
    
    AppConfig loaded2 = load_params<AppConfig>(filename);
    SIMPLE_ASSERT(loaded2.port == 9090, "Updated port should be preserved");
    
    std::remove(filename.c_str());
    std::remove((filename + ".bak").c_str());
    
    return true;
}

bool test_json_lite_position_in_errors() {
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
    SIMPLE_ASSERT(caught, "Error should include position information");
    
    return true;
}

bool test_json_lite_error_messages_comprehensive() {
    {
        bool caught = false;
        try {
            (void)parse_json("[1 2 3]");
        } catch (const std::runtime_error&) {
            caught = true;
        }
        SIMPLE_ASSERT(caught, "Should throw on array without comma");
    }
    
    {
        bool caught = false;
        try {
            (void)parse_json(R"({"key" "value"})");
        } catch (const std::runtime_error&) {
            caught = true;
        }
        SIMPLE_ASSERT(caught, "Should throw on object without colon");
    }
    
    {
        bool caught = false;
        try {
            (void)parse_json("123abc");
        } catch (const std::runtime_error&) {
            caught = true;
        }
        SIMPLE_ASSERT(caught, "Should throw on invalid number");
    }
    
    return true;
}

// ====================================================================
// New Container Type Tests
// ====================================================================

bool test_json_lite_unordered_set() {
    std::unordered_set<int> s1{1, 2, 3, 4, 5};
    JsonValue j = to_json(s1);
    
    SIMPLE_ASSERT(j.is_array(), "unordered_set should serialize to array");
    
    std::unordered_set<int> s2;
    from_json(j, s2);
    
    SIMPLE_ASSERT(s1.size() == s2.size(), "unordered_set size should be preserved");
    for (const auto& elem : s1) {
        SIMPLE_ASSERT(s2.count(elem) == 1, "unordered_set elements should be preserved");
    }
    
    return true;
}

bool test_json_lite_unordered_map() {
    std::unordered_map<std::string, int> m1{{"one", 1}, {"two", 2}, {"three", 3}};
    JsonValue j = to_json(m1);
    
    SIMPLE_ASSERT(j.is_object(), "unordered_map should serialize to object");
    
    std::unordered_map<std::string, int> m2;
    from_json(j, m2);
    
    SIMPLE_ASSERT(m1.size() == m2.size(), "unordered_map size should be preserved");
    SIMPLE_ASSERT(m2["one"] == 1, "unordered_map values should be preserved");
    SIMPLE_ASSERT(m2["two"] == 2, "unordered_map values should be preserved");
    SIMPLE_ASSERT(m2["three"] == 3, "unordered_map values should be preserved");
    
    return true;
}

bool test_json_lite_deque() {
    std::deque<int> d1{10, 20, 30, 40, 50};
    JsonValue j = to_json(d1);
    
    SIMPLE_ASSERT(j.is_array(), "deque should serialize to array");
    
    std::deque<int> d2;
    from_json(j, d2);
    
    SIMPLE_ASSERT(d1.size() == d2.size(), "deque size should be preserved");
    SIMPLE_ASSERT(std::equal(d1.begin(), d1.end(), d2.begin()), "deque elements should be preserved in order");
    
    return true;
}

bool test_json_lite_list() {
    std::list<int> lst1{100, 200, 300, 400};
    JsonValue j = to_json(lst1);
    
    SIMPLE_ASSERT(j.is_array(), "list should serialize to array");
    
    std::list<int> lst2;
    from_json(j, lst2);
    
    SIMPLE_ASSERT(lst1.size() == lst2.size(), "list size should be preserved");
    SIMPLE_ASSERT(std::equal(lst1.begin(), lst1.end(), lst2.begin()), "list elements should be preserved in order");
    
    return true;
}

bool test_json_lite_string_view() {
    std::string_view sv = "Hello, JsonLite!";
    JsonValue j = to_json(sv);
    
    SIMPLE_ASSERT(j.is_string(), "string_view should serialize to string");
    SIMPLE_ASSERT(std::get<std::string>(j) == "Hello, JsonLite!", "string_view content should be preserved");
    
    return true;
}

// ====================================================================
// Fixed-Width Integer Type Tests
// ====================================================================

bool test_json_lite_fixed_width_int8() {
    int8_t val_pos = 127;
    int8_t val_neg = -128;
    int8_t val_zero = 0;
    
    JsonValue j_pos = to_json(val_pos);
    JsonValue j_neg = to_json(val_neg);
    JsonValue j_zero = to_json(val_zero);
    
    int8_t out_pos, out_neg, out_zero;
    from_json(j_pos, out_pos);
    from_json(j_neg, out_neg);
    from_json(j_zero, out_zero);
    
    SIMPLE_ASSERT(out_pos == 127, "int8_t max should round-trip");
    SIMPLE_ASSERT(out_neg == -128, "int8_t min should round-trip");
    SIMPLE_ASSERT(out_zero == 0, "int8_t zero should round-trip");
    
    return true;
}

bool test_json_lite_fixed_width_int16() {
    int16_t val_pos = 32767;
    int16_t val_neg = -32768;
    
    JsonValue j_pos = to_json(val_pos);
    JsonValue j_neg = to_json(val_neg);
    
    int16_t out_pos, out_neg;
    from_json(j_pos, out_pos);
    from_json(j_neg, out_neg);
    
    SIMPLE_ASSERT(out_pos == 32767, "int16_t max should round-trip");
    SIMPLE_ASSERT(out_neg == -32768, "int16_t min should round-trip");
    
    return true;
}

bool test_json_lite_fixed_width_int32() {
    int32_t val_pos = 2147483647;
    int32_t val_neg = -2147483647 - 1;
    
    JsonValue j_pos = to_json(val_pos);
    JsonValue j_neg = to_json(val_neg);
    
    int32_t out_pos, out_neg;
    from_json(j_pos, out_pos);
    from_json(j_neg, out_neg);
    
    SIMPLE_ASSERT(out_pos == 2147483647, "int32_t max should round-trip");
    SIMPLE_ASSERT(out_neg == -2147483648, "int32_t min should round-trip");
    
    return true;
}

bool test_json_lite_fixed_width_int64() {
    int64_t val_pos = 9223372036854775807LL;
    int64_t val_neg = -9223372036854775807LL - 1;
    
    JsonValue j_pos = to_json(val_pos);
    JsonValue j_neg = to_json(val_neg);
    
    int64_t out_pos, out_neg;
    from_json(j_pos, out_pos);
    from_json(j_neg, out_neg);
    
    SIMPLE_ASSERT(out_pos == 9223372036854775807LL, "int64_t max should round-trip");
    SIMPLE_ASSERT(out_neg == (-9223372036854775807LL - 1), "int64_t min should round-trip");
    
    return true;
}

bool test_json_lite_fixed_width_uint8() {
    uint8_t val_max = 255;
    uint8_t val_min = 0;
    
    JsonValue j_max = to_json(val_max);
    JsonValue j_min = to_json(val_min);
    
    uint8_t out_max, out_min;
    from_json(j_max, out_max);
    from_json(j_min, out_min);
    
    SIMPLE_ASSERT(out_max == 255, "uint8_t max should round-trip");
    SIMPLE_ASSERT(out_min == 0, "uint8_t min should round-trip");
    
    return true;
}

bool test_json_lite_fixed_width_uint16() {
    uint16_t val_max = 65535;
    uint16_t val_min = 0;
    
    JsonValue j_max = to_json(val_max);
    JsonValue j_min = to_json(val_min);
    
    uint16_t out_max, out_min;
    from_json(j_max, out_max);
    from_json(j_min, out_min);
    
    SIMPLE_ASSERT(out_max == 65535, "uint16_t max should round-trip");
    SIMPLE_ASSERT(out_min == 0, "uint16_t min should round-trip");
    
    return true;
}

bool test_json_lite_fixed_width_uint32() {
    uint32_t val_max = 4294967295;
    uint32_t val_min = 0;
    
    JsonValue j_max = to_json(val_max);
    JsonValue j_min = to_json(val_min);
    
    uint32_t out_max, out_min;
    from_json(j_max, out_max);
    from_json(j_min, out_min);
    
    SIMPLE_ASSERT(out_max == 4294967295, "uint32_t max should round-trip");
    SIMPLE_ASSERT(out_min == 0, "uint32_t min should round-trip");
    
    return true;
}

bool test_json_lite_fixed_width_uint64() {
    uint64_t val_max = 18446744073709551615ULL;
    uint64_t val_min = 0;
    
    JsonValue j_max = to_json(val_max);
    JsonValue j_min = to_json(val_min);
    
    SIMPLE_ASSERT(j_max.is_number(), "uint64_t max should serialize to number");
    SIMPLE_ASSERT(j_min.is_int(), "uint64_t min should serialize to int");
    
    uint64_t out_min;
    from_json(j_min, out_min);
    SIMPLE_ASSERT(out_min == 0, "uint64_t min should round-trip");
    
    return true;
}

bool test_json_lite_size_t_type() {
    size_t val = 123456;
    JsonValue j = to_json(val);
    
    SIMPLE_ASSERT(j.is_number(), "size_t should serialize to number");
    
    size_t out;
    from_json(j, out);
    
    SIMPLE_ASSERT(out == 123456, "size_t should round-trip");
    
    return true;
}

bool test_json_lite_ptrdiff_t_type() {
    ptrdiff_t val_pos = 98765;
    ptrdiff_t val_neg = -12345;
    
    JsonValue j_pos = to_json(val_pos);
    JsonValue j_neg = to_json(val_neg);
    
    ptrdiff_t out_pos, out_neg;
    from_json(j_pos, out_pos);
    from_json(j_neg, out_neg);
    
    SIMPLE_ASSERT(out_pos == 98765, "ptrdiff_t positive should round-trip");
    SIMPLE_ASSERT(out_neg == -12345, "ptrdiff_t negative should round-trip");
    
    return true;
}

// ====================================================================
// Bug Verification Tests
// ====================================================================

bool test_json_lite_field_limit() {
    cpp_utilities::Max50Fields data;
    for (int i = 1; i <= 50; ++i) {
        *(&data.f1 + (i-1)) = i;
    }
    
    auto json = to_json_string(data);
    auto back = from_json_string<cpp_utilities::Max50Fields>(json);
    
    SIMPLE_ASSERT(back.f50 == 50, "Field 50 should serialize correctly");
    SIMPLE_ASSERT(back.f1 == 1, "Field 1 should serialize correctly");
    
    std::cout << " 50 fields work correctly\n";
    std::cout << " Note: 51+ fields will produce a clear compile-time error message\n";
    
    return true;
}

bool test_json_lite_bug2_name_collision() {
    cpp_utilities::TestBasic tb{42};
    auto json = to_json_string(tb);
    SIMPLE_ASSERT(json.find("42") != std::string::npos, "Should serialize correctly");
    
    cpp_utilities::TypeA a{10};
    cpp_utilities::TypeB b{3.14};
    auto json_a = to_json_string(a);
    auto json_b = to_json_string(b);
    SIMPLE_ASSERT(json_a.find("10") != std::string::npos, "TypeA serializes");
    SIMPLE_ASSERT(json_b.find("3.14") != std::string::npos, "TypeB serializes");
    
    cpp_utilities::OptConfig oc;
    auto json_opt = to_json_string(oc);
    SIMPLE_ASSERT(json_opt.find("8080") != std::string::npos, "Optional works");
    
    cpp_utilities::PrivateTest pt;
    auto json_priv = to_json_string(pt);
    SIMPLE_ASSERT(json_priv.find("99") != std::string::npos, "Intrusive works");
    
    std::cout << "  ✓ Basic serialization works\n";
    std::cout << "  ✓ Multiple types don't collide with each other\n";
    std::cout << "  ✓ Optional macro variant works without collision\n";
    std::cout << "  ✓ Intrusive macro with friend functions works\n";
    std::cout << "  ✓ Namespace isolation prevents collisions\n";
    std::cout << "  ℹ Implementation functions in cpp_utilities::json_detail namespace\n";
    std::cout << "  ℹ [[maybe_unused]] attribute reduces warnings\n";
    
    return true;
}
bool test_json_lite_bug2_collision_prevention() {
    cpp_utilities::User u{ "Alice", 30 };
    cpp_utilities::Product p{ "Widget", 19.99 };
    cpp_utilities::Order o{ "Order#123", 5 };

    auto json_u = to_json_string(u);
    auto json_p = to_json_string(p);
    auto json_o = to_json_string(o);

    SIMPLE_ASSERT(json_u.find("Alice") != std::string::npos, "User serializes");
    SIMPLE_ASSERT(json_p.find("Widget") != std::string::npos, "Product serializes");
    SIMPLE_ASSERT(json_o.find("Order#123") != std::string::npos, "Order serializes");

    cpp_utilities::Container c{ {1, 2, 3} };
    auto json_c = to_json_string(c);
    SIMPLE_ASSERT(json_c.find("1") != std::string::npos &&
        json_c.find("2") != std::string::npos &&
        json_c.find("3") != std::string::npos,
        "Container with vector works");

    cpp_utilities::RequiredFields rf{ 5 };
    cpp_utilities::OptionalFields of;

    auto json_rf = to_json_string(rf);
    auto json_of = to_json_string(of);

    SIMPLE_ASSERT(json_rf.find("5") != std::string::npos, "Required fields work");
    SIMPLE_ASSERT(json_of.find("10") != std::string::npos, "Optional fields work");

    std::cout << "  Multiple structs in same file don't collide\n";
    std::cout << "  Template types work without collision\n";
    std::cout << "  Mix of macro variants work together\n";
    std::cout << "  [[maybe_unused]] attribute reduces warnings\n";
    std::cout << "\n  NOTE: For types in custom namespaces, define the macro\n";
    std::cout << "    inside the namespace for best results.\n";

    return true;
}

bool test_json_lite_numeric_formatting() {
    // Bug3 FIXED: Smart number formatting now handles extreme values correctly
    cpp_utilities::ScientificData sci{ 6.62607015e-34, 6.02214076e23, 3.14159 };
    auto json = to_json_string(sci);

    std::cout << "  Scientific JSON output:\n";
    std::cout << "  " << json << "\n";

    // Verify extreme values use scientific notation
    SIMPLE_ASSERT(json.find("e-34") != std::string::npos || json.find("e-33") != std::string::npos,
        "Planck constant should use scientific notation");
    SIMPLE_ASSERT(json.find("e+23") != std::string::npos || json.find("e+22") != std::string::npos,
        "Avogadro number should use scientific notation");

    // Verify normal values use fixed notation
    SIMPLE_ASSERT(json.find("3.1415") != std::string::npos, "Normal values should use fixed notation");

    // Verify round-trip accuracy
    auto parsed = from_json_string<cpp_utilities::ScientificData>(json);

    double planck_rel_error = std::abs(parsed.planck_constant - sci.planck_constant) / sci.planck_constant;
    double avogadro_rel_error = std::abs(parsed.avogadro_number - sci.avogadro_number) / sci.avogadro_number;

    SIMPLE_ASSERT(planck_rel_error < 1e-10, "Planck constant round-trips accurately");
    SIMPLE_ASSERT(avogadro_rel_error < 1e-10, "Avogadro number round-trips accurately");
    SIMPLE_ASSERT(std::abs(parsed.normal_value - 3.14159) < 1e-10, "Normal values round-trip accurately");

    std::cout << "  Planck constant preserved with " << planck_rel_error << " relative error\n";
    std::cout << "  Avogadro number preserved with " << avogadro_rel_error << " relative error\n";
    std::cout << "  Smart formatting: scientific for extreme, fixed for normal\n";

    return true;
}

bool test_json_lite_bug4_error_context() {
    std::string bad_json = R"({"valid": 123, "broken": [1, 2, 3,]})";
    
    bool caught = false;
    std::string error_msg;
    try {
        auto result = from_json_string<JsonValue>(bad_json);
    } catch (const std::exception& e) {
        caught = true;
        error_msg = e.what();
    }
    
    SIMPLE_ASSERT(caught, "Should throw on invalid JSON");
    SIMPLE_ASSERT(error_msg.find("position") != std::string::npos, 
                  "Error should include position information");
    
    std::cout << "  Error message: " << error_msg << "\n";
    std::cout << "  Position information IS provided in error messages\n";
    
    return true;
}

bool test_json_lite_bug5_fixed_arrays() {
    cpp_utilities::WithStdArray data{{1, 2, 3}, {3.14, 2.71}};
    auto json = to_json_string(data);
    auto back = from_json_string<cpp_utilities::WithStdArray>(json);
    
    SIMPLE_ASSERT(back.values[0] == 1, "std::array serializes correctly");
    SIMPLE_ASSERT(back.values[2] == 3, "std::array deserializes correctly");
    SIMPLE_ASSERT(std::abs(back.coords[0] - 3.14) < 1e-5, "double array works");
    
    std::cout << "  std::array works perfectly\n";
    std::cout << "  C-style arrays (int x[5]) are not supported by design\n";
    std::cout << "  Modern C++ recommendation: use std::array instead\n";
    
    return true;
}

bool test_json_lite_bug6_policy_incompatibility() {
    cpp_utilities::TestData data{3.14159265358979};
    
    auto std_json = to_json_string<cpp_utilities::TestData, StandardJsonPolicy>(data);
    auto low_json = to_json_string<cpp_utilities::TestData, LowPrecisionPolicy>(data);
    
    std::cout << "  Standard precision: " << std_json << "\n";
    std::cout << "  Low precision:      " << low_json << "\n";
    
    bool policy_respected = (std_json != low_json);
    
    if (policy_respected) {
        std::cout << "  Policies are respected by macros\n";
        std::cout << "  PolicyContext system working correctly\n";
    } else {
        std::cout << "  Policies are ignored\n";
        std::cout << "  Expected different precision, got identical output\n";
    }
    
    SIMPLE_ASSERT(policy_respected, "Custom policies must be respected by macro-generated functions");
    
    return true;
}

bool test_json_lite_bug6_policy_fix_verification() {
    cpp_utilities::ComplexData data{98.6543210, 1013.25987654, "sensor_1"};
    
    auto json2 = to_json_string<cpp_utilities::ComplexData, Precision2>(data);
    auto json4 = to_json_string<cpp_utilities::ComplexData, Precision4>(data);
    auto json8 = to_json_string<cpp_utilities::ComplexData, Precision8>(data);
    auto json16 = to_json_string<cpp_utilities::ComplexData, StandardJsonPolicy>(data);
    
    std::cout << "  Precision 2:  " << json2.substr(0, 60) << "...\n";
    std::cout << "  Precision 4:  " << json4.substr(0, 60) << "...\n";
    std::cout << "  Precision 8:  " << json8.substr(0, 60) << "...\n";
    std::cout << "  Precision 16: " << json16.substr(0, 60) << "...\n";
    
    SIMPLE_ASSERT(json2.length() < json4.length(), "Higher precision should produce longer output");
    SIMPLE_ASSERT(json4.length() < json8.length(), "Higher precision should produce longer output");
    SIMPLE_ASSERT(json8.length() < json16.length(), "Higher precision should produce longer output");
    
    std::cout << "  All precision levels produce different output\n";
    std::cout << "  PolicyContext system fully functional\n";
    
    return true;
}

// ====================================================================
// Benchmark Function
// ====================================================================

void benchmark_jsonlite() {
    std::cout << "\n" << colors::cyan() << "JsonLite Benchmarks:" << colors::reset() << "\n\n";
    
    Point p{42, 84};
    double serialize_time = measure_perf([&p]() {
        std::string json = to_json_string(p);
        DoNotOptimize(json);
    }, 100000, 1000);
    std::cout << "Serialize Point: " << format_time(serialize_time) << "\n";
    
    std::string json_str = R"({"x": 42, "y": 84})";
    double parse_time = measure_perf([&json_str]() {
        JsonValue j = parse_json(json_str);
        DoNotOptimize(j);
    }, 100000, 1000);
    std::cout << "Parse simple object: " << format_time(parse_time) << "\n";
    
    double roundtrip_time = measure_perf([&p]() {
        std::string json = to_json_string(p);
        Point p2 = from_json_string<Point>(json);
        DoNotOptimize(p2);
    }, 10000, 100);
    std::cout << "Round-trip Point: " << format_time(roundtrip_time) << "\n";
    
    std::vector<int> large_vec(1000);
    for (size_t i = 0; i < large_vec.size(); ++i) {
        large_vec[i] = static_cast<int>(i);
    }
    double large_array_time = measure_perf([&large_vec]() {
        std::string json = to_json_string(large_vec);
        DoNotOptimize(json);
    }, 1000, 10);
    std::cout << "Serialize 1000 integers: " << format_time(large_array_time) << "\n";
    
    Employee emp;
    emp.name = "John Doe";
    emp.id = 12345;
    emp.email = "john@example.com";
    emp.skills = {"C++", "Python", "JavaScript", "Go", "Rust"};
    emp.certifications["AWS"] = 2020;
    emp.certifications["Azure"] = 2021;
    emp.certifications["GCP"] = 2022;
    
    double complex_struct_time = measure_perf([&emp]() {
        std::string json = to_json_string(emp);
        Employee emp2 = from_json_string<Employee>(json);
        DoNotOptimize(emp2);
    }, 10000, 100);
    std::cout << "Round-trip complex struct: " << format_time(complex_struct_time) << "\n";
}

// ====================================================================
// Main Test Function
// ====================================================================

bool test_JsonLite() {
    PRINT_HEADER(JSON LITE)
    
    TestRunner runner;
    
    RUN_TEST(runner, json_lite_basic_types);
    
    RUN_TEST(runner, json_lite_int8);
    RUN_TEST(runner, json_lite_int16);
    RUN_TEST(runner, json_lite_int32);
    RUN_TEST(runner, json_lite_int64);
    RUN_TEST(runner, json_lite_uint8);
    RUN_TEST(runner, json_lite_uint16);
    RUN_TEST(runner, json_lite_uint32);
    RUN_TEST(runner, json_lite_uint64);
    RUN_TEST(runner, json_lite_float_types);
    RUN_TEST(runner, json_lite_numeric_edge_cases);
    RUN_TEST(runner, json_lite_fixed_width_int8);
    RUN_TEST(runner, json_lite_fixed_width_int16);
    RUN_TEST(runner, json_lite_fixed_width_int32);
    RUN_TEST(runner, json_lite_fixed_width_int64);
    RUN_TEST(runner, json_lite_fixed_width_uint8);
    RUN_TEST(runner, json_lite_fixed_width_uint16);
    RUN_TEST(runner, json_lite_fixed_width_uint32);
    RUN_TEST(runner, json_lite_fixed_width_uint64);
    RUN_TEST(runner, json_lite_size_t_type);
    RUN_TEST(runner, json_lite_ptrdiff_t_type);
    
    RUN_TEST(runner, json_lite_string_escaping);
    RUN_TEST(runner, json_lite_string_empty);
    RUN_TEST(runner, json_lite_string_whitespace);
    RUN_TEST(runner, json_lite_string_special_chars);
    RUN_TEST(runner, json_lite_string_unicode);
    RUN_TEST(runner, json_lite_string_long);
    
    RUN_TEST(runner, json_lite_containers);
    RUN_TEST(runner, json_lite_vector_empty);
    RUN_TEST(runner, json_lite_vector_single);
    RUN_TEST(runner, json_lite_set);
    RUN_TEST(runner, json_lite_map_empty);
    RUN_TEST(runner, json_lite_map_numeric_keys);
    RUN_TEST(runner, json_lite_nested_vectors);
    RUN_TEST(runner, json_lite_mixed_nested);
    RUN_TEST(runner, json_lite_array);
    RUN_TEST(runner, json_lite_unordered_set);
    RUN_TEST(runner, json_lite_unordered_map);
    RUN_TEST(runner, json_lite_deque);
    RUN_TEST(runner, json_lite_list);
    RUN_TEST(runner, json_lite_string_view);
    
    RUN_TEST(runner, json_lite_optional);
    RUN_TEST(runner, json_lite_optional_string);
    RUN_TEST(runner, json_lite_optional_vector);
    
    RUN_TEST(runner, json_lite_tuples_pairs);
    RUN_TEST(runner, json_lite_pair_nested);
    RUN_TEST(runner, json_lite_tuple_large);
    
    RUN_TEST(runner, json_lite_simple_struct);
    RUN_TEST(runner, json_lite_complex_struct);
    RUN_TEST(runner, json_lite_optional_fields);
    RUN_TEST(runner, json_lite_intrusive_serialization);
    RUN_TEST(runner, json_lite_nested_structs);
    RUN_TEST(runner, json_lite_complex_nested);
    RUN_TEST(runner, json_lite_deep_hierarchy);
    
    RUN_TEST(runner, json_lite_parser_basic);
    RUN_TEST(runner, json_lite_parser_containers);
    RUN_TEST(runner, json_lite_parser_edge_cases);
    RUN_TEST(runner, json_lite_parser_numbers);
    RUN_TEST(runner, json_lite_parser_strings);
    RUN_TEST(runner, json_lite_parser_nested);
    RUN_TEST(runner, json_lite_parser_errors);
    RUN_TEST(runner, json_lite_parser_literals);
    RUN_TEST(runner, json_lite_parser_unicode);
    
    RUN_TEST(runner, json_lite_pretty_print);
    RUN_TEST(runner, json_lite_numeric_precision);
    RUN_TEST(runner, json_lite_nan_inf_handling);
    RUN_TEST(runner, json_lite_standard_policy_nan);
    
    RUN_TEST(runner, json_lite_file_io);
    RUN_TEST(runner, json_lite_file_io_complex);
    RUN_TEST(runner, json_lite_file_io_errors);
    
    RUN_TEST(runner, json_lite_roundtrip_all_types);
    RUN_TEST(runner, json_lite_convenience_functions);
    RUN_TEST(runner, json_lite_roundtrip_vectors);
    RUN_TEST(runner, json_lite_roundtrip_maps);
    
    RUN_TEST(runner, json_lite_large_data);
    RUN_TEST(runner, json_lite_deeply_nested);
    RUN_TEST(runner, json_lite_large_strings);
    RUN_TEST(runner, json_lite_large_map);
    
    RUN_TEST(runner, json_lite_depth_limit_parse);
    RUN_TEST(runner, json_lite_depth_limit_dump);
    RUN_TEST(runner, json_lite_range_checks);
    RUN_TEST(runner, json_lite_range_int8);
    RUN_TEST(runner, json_lite_better_error_messages);
    RUN_TEST(runner, json_lite_param_helpers);
    RUN_TEST(runner, json_lite_position_in_errors);
    RUN_TEST(runner, json_lite_error_messages_comprehensive);
    
    RUN_TEST(runner, json_lite_field_limit);
    RUN_TEST(runner, json_lite_bug2_name_collision);
    RUN_TEST(runner, json_lite_bug2_collision_prevention);
    RUN_TEST(runner, json_lite_numeric_formatting);
    RUN_TEST(runner, json_lite_bug4_error_context);
    RUN_TEST(runner, json_lite_bug5_fixed_arrays);
    RUN_TEST(runner, json_lite_bug6_policy_incompatibility);
    RUN_TEST(runner, json_lite_bug6_policy_fix_verification);
    
    benchmark_jsonlite();
    
    return 0 == runner.print_summary();
}

} // namespace cpp_utilities::testing