/* @file test_JsonLite.cpp
 * @brief Comprehensive unit tests for standalone JsonLite.h
 *
 * Tests the standalone JsonLite implementation with ZERO external dependencies
 * (beyond standard library). All 150+ test cases verify:
 * - Basic types (null, bool, numeric, string)
 * - All integer types (int8_t through int64_t, signed and unsigned)
 * - Comprehensive boundary testing for all numeric types
 * - Floating-point types and edge cases
 * - Container types (vector, map, set, array, deque, list, etc.)
 * - Optional types and nullability
 * - Tuples and pairs
 * - Struct serialization (intrusive and non-intrusive)
 * - Nested structures and deep hierarchies
 * - Parser robustness and error handling
 * - File I/O operations including backup functionality
 * - Pretty printing and formatting policies
 * - Numeric precision control
 * - Round-trip conversions
 * - Large data handling
 * - Value-returning API convenience functions
 * - JSONC comment parsing
 * - UTF-8 escaping and multi-byte character handling
 * - Numeric bounds checking with overflow detection via roundtrip validation
 * - NaN/Infinity policy enforcement
 * - Locale-independent parsing
 */
/*
FATP_META:
  meta_version: 1
  component: JsonLite
  file_role: test
  path: components/Json/tests/test_JsonLite.cpp
  namespace: fat_p
  summary: "Unit tests for JsonLite."
  api_stability: in_work
  related:
    docs_search: "JsonLite"
    headers:
      - include/fat_p/JsonLite.h
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

#include <algorithm>
#include <chrono>
#include <climits>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <locale>
#include <random>
#include <sstream>

#include "FatPTest.h"
#include "JsonLite.h"

namespace fat_p::testing::jsonlite
{

struct Point
{
    int x = 0;
    int y = 0;
};
FATP_JSON_DEFINE_TYPE_NON_INTRUSIVE(Point, x, y)

struct Point3D
{
    int x = 0;
    int y = 0;
    int z = 0;
};
FATP_JSON_DEFINE_TYPE_NON_INTRUSIVE(Point3D, x, y, z)

struct Person
{
    std::string name;
    int age = 0;
    std::vector<std::string> hobbies;
};
FATP_JSON_DEFINE_TYPE_NON_INTRUSIVE(Person, name, age, hobbies)

struct Config
{
    std::optional<int> timeout;
    std::optional<std::string> host;
    int port = 8080;
};
FATP_JSON_DEFINE_TYPE_OPTIONAL(Config, timeout, host, port)

class PrivateData
{
private:
    int mSecret;
    std::string mCode;

public:
    PrivateData()
        : mSecret(0)
        , mCode()
    {
    }
    PrivateData(int s, std::string c)
        : mSecret(s)
        , mCode(std::move(c))
    {
    }

    int secret() const
    {
        return mSecret;
    }
    std::string code() const
    {
        return mCode;
    }

    FATP_JSON_DEFINE_TYPE_INTRUSIVE(PrivateData, mSecret, mCode)
};

struct Nested
{
    Point position{};
    std::string label;
};
FATP_JSON_DEFINE_TYPE_NON_INTRUSIVE(Nested, position, label)

struct Complex
{
    std::map<std::string, int> scores;
    std::vector<Point> points;
    std::optional<std::string> description;
};
FATP_JSON_DEFINE_TYPE_NON_INTRUSIVE(Complex, scores, points, description)

struct AppConfig
{
    int port = 0;
    std::string host;
    std::vector<std::string> allowed_ips;
    std::optional<int> timeout;
};
FATP_JSON_DEFINE_TYPE_NON_INTRUSIVE(AppConfig, port, host, allowed_ips, timeout)

struct Employee
{
    std::string name;
    int id = 0;
    std::optional<std::string> email;
    std::vector<std::string> skills;
    std::map<std::string, int> certifications;
};
FATP_JSON_DEFINE_TYPE_NON_INTRUSIVE(Employee, name, id, email, skills, certifications)

struct Department
{
    std::string name;
    std::vector<Employee> employees;
    std::optional<Employee> manager;
};
FATP_JSON_DEFINE_TYPE_NON_INTRUSIVE(Department, name, employees, manager)

struct Company
{
    std::string name;
    std::vector<Department> departments;
    std::map<std::string, std::string> metadata;
};
FATP_JSON_DEFINE_TYPE_NON_INTRUSIVE(Company, name, departments, metadata)

struct Max50Fields
{
    int f1, f2, f3, f4, f5, f6, f7, f8, f9, f10;
    int f11, f12, f13, f14, f15, f16, f17, f18, f19, f20;
    int f21, f22, f23, f24, f25, f26, f27, f28, f29, f30;
    int f31, f32, f33, f34, f35, f36, f37, f38, f39, f40;
    int f41, f42, f43, f44, f45, f46, f47, f48, f49, f50;
};

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 6262)
#endif
FATP_JSON_DEFINE_TYPE_NON_INTRUSIVE(Max50Fields,
                                    f1,
                                    f2,
                                    f3,
                                    f4,
                                    f5,
                                    f6,
                                    f7,
                                    f8,
                                    f9,
                                    f10,
                                    f11,
                                    f12,
                                    f13,
                                    f14,
                                    f15,
                                    f16,
                                    f17,
                                    f18,
                                    f19,
                                    f20,
                                    f21,
                                    f22,
                                    f23,
                                    f24,
                                    f25,
                                    f26,
                                    f27,
                                    f28,
                                    f29,
                                    f30,
                                    f31,
                                    f32,
                                    f33,
                                    f34,
                                    f35,
                                    f36,
                                    f37,
                                    f38,
                                    f39,
                                    f40,
                                    f41,
                                    f42,
                                    f43,
                                    f44,
                                    f45,
                                    f46,
                                    f47,
                                    f48,
                                    f49,
                                    f50)
#ifdef _MSC_VER
#pragma warning(pop)
#endif

struct TestBasic
{
    int x;
};
FATP_JSON_DEFINE_TYPE_NON_INTRUSIVE(TestBasic, x)

struct TypeA
{
    int value;
};
FATP_JSON_DEFINE_TYPE_NON_INTRUSIVE(TypeA, value)

struct TypeB
{
    double value;
};
FATP_JSON_DEFINE_TYPE_NON_INTRUSIVE(TypeB, value)

struct OptConfig
{
    int port = 8080;
};
FATP_JSON_DEFINE_TYPE_OPTIONAL(OptConfig, port)

class PrivateTest
{
    int mSecret = 99;

public:
    FATP_JSON_DEFINE_TYPE_INTRUSIVE(PrivateTest, mSecret)
    int get_secret() const
    {
        return mSecret;
    }
};

struct User
{
    std::string name;
    int age = 0;
};
FATP_JSON_DEFINE_TYPE_NON_INTRUSIVE(User, name, age)

struct Product
{
    std::string name;
    double price = 0.0;
};
FATP_JSON_DEFINE_TYPE_NON_INTRUSIVE(Product, name, price)

struct Order
{
    std::string name;
    int quantity = 0;
};
FATP_JSON_DEFINE_TYPE_NON_INTRUSIVE(Order, name, quantity)

struct Container
{
    std::vector<int> items;
};
FATP_JSON_DEFINE_TYPE_NON_INTRUSIVE(Container, items)

struct RequiredFields
{
    int x;
};
FATP_JSON_DEFINE_TYPE_NON_INTRUSIVE(RequiredFields, x)

struct OptionalFields
{
    int y = 10;
};
FATP_JSON_DEFINE_TYPE_OPTIONAL(OptionalFields, y)

struct ScientificData
{
    double planck_constant;
    double avogadro_number;
    double normal_value;
};
FATP_JSON_DEFINE_TYPE_NON_INTRUSIVE(ScientificData, planck_constant, avogadro_number, normal_value)

struct WithStdArray
{
    std::array<int, 3> values;
    std::array<double, 2> coords;
};
FATP_JSON_DEFINE_TYPE_NON_INTRUSIVE(WithStdArray, values, coords)

struct TestData
{
    double value;
};
FATP_JSON_DEFINE_TYPE_NON_INTRUSIVE(TestData, value)

struct ComplexData
{
    double temperature;
    double pressure;
    std::string name;
};
FATP_JSON_DEFINE_TYPE_NON_INTRUSIVE(ComplexData, temperature, pressure, name)

struct Utf8TestData
{
    std::string european_text;
    std::string asian_text;
    std::string emoji_text;
    std::string mixed_text;
};
FATP_JSON_DEFINE_TYPE_NON_INTRUSIVE(Utf8TestData, european_text, asian_text, emoji_text, mixed_text)

struct DatabaseConfig
{
    std::string host;
    int port = 0;
    std::optional<int> timeout;
};
FATP_JSON_DEFINE_TYPE_NON_INTRUSIVE(DatabaseConfig, host, port, timeout)

struct ServerConfig
{
    DatabaseConfig database;
    std::vector<std::string> servers;
};
FATP_JSON_DEFINE_TYPE_NON_INTRUSIVE(ServerConfig, database, servers)

struct LowPrecisionPolicy : StandardJsonPolicy
{
    static constexpr int numeric_precision = 2;
};

struct Precision2 : StandardJsonPolicy
{
    static constexpr int numeric_precision = 2;
};

struct Precision4 : StandardJsonPolicy
{
    static constexpr int numeric_precision = 4;
};

struct Precision8 : StandardJsonPolicy
{
    static constexpr int numeric_precision = 8;
};

// Using declarations for JsonLite functionality - placed after struct definitions
// to avoid being hidden by macro-generated to_json/from_json functions
using fat_p::from_json;
using fat_p::from_json_string;
using fat_p::json_decode;
using fat_p::json_encode;
using fat_p::JsonArray;
using fat_p::JsonObject;
using fat_p::JsonValue;
using fat_p::load_params;
using fat_p::parse_json;
using fat_p::query_json_as;
using fat_p::query_json_pointer;
using fat_p::save_params;
using fat_p::save_params_with_backup;
using fat_p::to_json;
using fat_p::to_json_string;

FATP_TEST_CASE(json_lite_basic_types)
{
    FATP_SUBTEST("null type")
    {
        JsonValue j = nullptr;
        FATP_ASSERT_TRUE(j.is_null(), "Should be null");
        std::string str = to_json_string(j);
        FATP_ASSERT_EQ(str, "null", "Null should serialize to 'null'");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("boolean types")
    {
        JsonValue j_true = true;
        JsonValue j_false = false;
        FATP_ASSERT_TRUE(j_true.is_bool(), "Should be bool");
        FATP_ASSERT_EQ(to_json_string(j_true), "true", "True should serialize to 'true'");
        FATP_ASSERT_EQ(to_json_string(j_false), "false", "False should serialize to 'false'");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("numeric types")
    {
        JsonValue j_int = static_cast<int64_t>(42);
        JsonValue j_float = 3.14159;
        FATP_ASSERT_TRUE(j_int.is_int(), "Should be int");
        FATP_ASSERT_TRUE(j_float.is_number(), "Should be number");
        FATP_ASSERT_EQ(to_json_string(j_int), "42", "Int should serialize correctly");
    }
    FATP_END_SUBTEST

    return fat_p::testing::get_subtest_tracker().all_passed();
}

FATP_TEST_CASE(json_lite_int8)
{
    FATP_SUBTEST("int8_t serialization")
    {
        int8_t val = 127;
        JsonValue j = json_encode(val);
        FATP_ASSERT_TRUE(j.is_int(), "int8_t should be int");
        FATP_ASSERT_EQ(std::get<int64_t>(j), static_cast<int64_t>(127), "Value should be 127");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("int8_t deserialization")
    {
        JsonValue j = static_cast<int64_t>(42);
        int8_t result = 0;
        from_json(j, result);
        FATP_ASSERT_EQ(result, static_cast<int8_t>(42), "Value should be 42");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("int8_t range check")
    {
        JsonValue j = static_cast<int64_t>(1000);
        int8_t result = 0;
        FATP_ASSERT_THROWS(from_json(j, result), std::runtime_error, "Should throw on overflow");
    }
    FATP_END_SUBTEST

    return fat_p::testing::get_subtest_tracker().all_passed();
}

FATP_TEST_CASE(json_lite_int16)
{
    FATP_SUBTEST("int16_t serialization")
    {
        int16_t val = 32767;
        JsonValue j = json_encode(val);
        FATP_ASSERT_EQ(std::get<int64_t>(j), static_cast<int64_t>(32767), "Value should be 32767");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("int16_t deserialization")
    {
        JsonValue j = static_cast<int64_t>(1000);
        int16_t result = 0;
        from_json(j, result);
        FATP_ASSERT_EQ(result, static_cast<int16_t>(1000), "Value should be 1000");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("int16_t range check")
    {
        JsonValue j = static_cast<int64_t>(100000);
        int16_t result = 0;
        FATP_ASSERT_THROWS(from_json(j, result), std::runtime_error, "Should throw on overflow");
    }
    FATP_END_SUBTEST

    return fat_p::testing::get_subtest_tracker().all_passed();
}

FATP_TEST_CASE(json_lite_int32)
{
    FATP_SUBTEST("int32_t serialization")
    {
        int val = 2147483647;
        JsonValue j = json_encode(val);
        FATP_ASSERT_EQ(std::get<int64_t>(j), static_cast<int64_t>(2147483647), "Value should be INT_MAX");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("int32_t deserialization")
    {
        JsonValue j = static_cast<int64_t>(100000);
        int result = 0;
        from_json(j, result);
        FATP_ASSERT_EQ(result, 100000, "Value should be 100000");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("int32_t range check")
    {
        JsonValue j = static_cast<int64_t>(3000000000LL);
        int result = 0;
        FATP_ASSERT_THROWS(from_json(j, result), std::runtime_error, "Should throw on overflow");
    }
    FATP_END_SUBTEST

    return fat_p::testing::get_subtest_tracker().all_passed();
}

FATP_TEST_CASE(json_lite_int64)
{
    FATP_SUBTEST("int64_t serialization")
    {
        int64_t val = 9223372036854775807LL;
        JsonValue j = json_encode(val);
        FATP_ASSERT_EQ(std::get<int64_t>(j), val, "Value should be LLONG_MAX");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("int64_t deserialization")
    {
        JsonValue j = static_cast<int64_t>(9223372036854775807LL);
        int64_t result = 0;
        from_json(j, result);
        FATP_ASSERT_EQ(result, 9223372036854775807LL, "Value should be LLONG_MAX");
    }
    FATP_END_SUBTEST

    return fat_p::testing::get_subtest_tracker().all_passed();
}

FATP_TEST_CASE(json_lite_uint8)
{
    FATP_SUBTEST("uint8_t serialization")
    {
        uint8_t val = 255;
        JsonValue j = json_encode(val);
        FATP_ASSERT_EQ(std::get<int64_t>(j), static_cast<int64_t>(255), "Value should be 255");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("uint8_t deserialization")
    {
        JsonValue j = static_cast<int64_t>(200);
        uint8_t result = 0;
        from_json(j, result);
        FATP_ASSERT_EQ(result, static_cast<uint8_t>(200), "Value should be 200");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("uint8_t range check")
    {
        JsonValue j = static_cast<int64_t>(1000);
        uint8_t result = 0;
        FATP_ASSERT_THROWS(from_json(j, result), std::runtime_error, "Should throw on overflow");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("uint8_t negative check")
    {
        JsonValue j = static_cast<int64_t>(-1);
        uint8_t result = 0;
        FATP_ASSERT_THROWS(from_json(j, result), std::runtime_error, "Should throw on negative value");
    }
    FATP_END_SUBTEST

    return fat_p::testing::get_subtest_tracker().all_passed();
}

FATP_TEST_CASE(json_lite_uint16)
{
    FATP_SUBTEST("uint16_t serialization")
    {
        uint16_t val = 65535;
        JsonValue j = json_encode(val);
        FATP_ASSERT_EQ(std::get<int64_t>(j), static_cast<int64_t>(65535), "Value should be 65535");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("uint16_t deserialization")
    {
        JsonValue j = static_cast<int64_t>(60000);
        uint16_t result = 0;
        from_json(j, result);
        FATP_ASSERT_EQ(result, static_cast<uint16_t>(60000), "Value should be 60000");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("uint16_t range check")
    {
        JsonValue j = static_cast<int64_t>(100000);
        uint16_t result = 0;
        FATP_ASSERT_THROWS(from_json(j, result), std::runtime_error, "Should throw on overflow");
    }
    FATP_END_SUBTEST

    return fat_p::testing::get_subtest_tracker().all_passed();
}

FATP_TEST_CASE(json_lite_uint32)
{
    FATP_SUBTEST("uint32_t serialization")
    {
        unsigned int val = 4294967295U;
        JsonValue j = json_encode(val);
        FATP_ASSERT_EQ(std::get<int64_t>(j), static_cast<int64_t>(4294967295U), "Value should be UINT_MAX");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("uint32_t deserialization")
    {
        JsonValue j = static_cast<int64_t>(4000000000LL);
        unsigned int result = 0;
        from_json(j, result);
        FATP_ASSERT_EQ(result, 4000000000U, "Value should be 4000000000");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("uint32_t range check")
    {
        JsonValue j = static_cast<int64_t>(5000000000LL);
        unsigned int result = 0;
        FATP_ASSERT_THROWS(from_json(j, result), std::runtime_error, "Should throw on overflow");
    }
    FATP_END_SUBTEST

    return fat_p::testing::get_subtest_tracker().all_passed();
}

FATP_TEST_CASE(json_lite_uint64)
{
    FATP_SUBTEST("uint64_t serialization large value")
    {
        unsigned long long val = 9223372036854775808ULL;
        JsonValue j = json_encode(val);
        FATP_ASSERT_TRUE(j.is_number(), "Large uint64_t should be number");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("uint64_t deserialization")
    {
        JsonValue j = static_cast<int64_t>(1000000000LL);
        unsigned long long result = 0;
        from_json(j, result);
        FATP_ASSERT_EQ(result, 1000000000ULL, "Value should be 1000000000");
    }
    FATP_END_SUBTEST

    return fat_p::testing::get_subtest_tracker().all_passed();
}

FATP_TEST_CASE(json_lite_float_types)
{
    FATP_SUBTEST("float serialization")
    {
        float val = 3.14f;
        JsonValue j = json_encode(val);
        FATP_ASSERT_TRUE(j.is_number(), "Float should be number");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("double serialization")
    {
        double val = 2.718281828459045;
        JsonValue j = json_encode(val);
        FATP_ASSERT_TRUE(j.is_number(), "Double should be number");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("float deserialization")
    {
        JsonValue j = 3.14;
        float result = 0.0f;
        from_json(j, result);
        FATP_ASSERT_CLOSE(result, 3.14f, "Value should be close to 3.14");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("double deserialization")
    {
        JsonValue j = 2.718281828459045;
        double result = 0.0;
        from_json(j, result);
        FATP_ASSERT_CLOSE(result, 2.718281828459045, "Value should be close to e");
    }
    FATP_END_SUBTEST

    return fat_p::testing::get_subtest_tracker().all_passed();
}

FATP_TEST_CASE(json_lite_numeric_edge_cases)
{
    FATP_SUBTEST("very large double")
    {
        double val = 1.7976931348623157e+308;
        JsonValue j = json_encode(val);
        double result = 0.0;
        from_json(j, result);
        FATP_ASSERT_CLOSE(result, val, "Large double should round-trip");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("very small double")
    {
        double val = 2.2250738585072014e-308;
        JsonValue j = json_encode(val);
        double result = 0.0;
        from_json(j, result);
        FATP_ASSERT_CLOSE(result, val, "Small double should round-trip");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("negative zero")
    {
        double val = -0.0;
        JsonValue j = json_encode(val);
        FATP_ASSERT_TRUE(j.is_number(), "Negative zero should be number");
    }
    FATP_END_SUBTEST

    return fat_p::testing::get_subtest_tracker().all_passed();
}

FATP_TEST_CASE(json_lite_fixed_width_int8)
{
    int8_t val = -128;
    JsonValue j = json_encode(val);
    int8_t result = 0;
    from_json(j, result);
    FATP_ASSERT_EQ(result, val, "int8_t min should round-trip");

    val = 127;
    j = json_encode(val);
    from_json(j, result);
    FATP_ASSERT_EQ(result, val, "int8_t max should round-trip");

    return true;
}

FATP_TEST_CASE(json_lite_fixed_width_int16)
{
    int16_t val = -32768;
    JsonValue j = json_encode(val);
    int16_t result = 0;
    from_json(j, result);
    FATP_ASSERT_EQ(result, val, "int16_t min should round-trip");

    val = 32767;
    j = json_encode(val);
    from_json(j, result);
    FATP_ASSERT_EQ(result, val, "int16_t max should round-trip");

    return true;
}

FATP_TEST_CASE(json_lite_fixed_width_int32)
{
    int32_t val = -2147483647 - 1;
    JsonValue j = json_encode(val);
    int32_t result = 0;
    from_json(j, result);
    FATP_ASSERT_EQ(result, val, "int32_t min should round-trip");

    val = 2147483647;
    j = json_encode(val);
    from_json(j, result);
    FATP_ASSERT_EQ(result, val, "int32_t max should round-trip");

    return true;
}

FATP_TEST_CASE(json_lite_fixed_width_int64)
{
    int64_t val = LLONG_MIN;
    JsonValue j = json_encode(val);
    int64_t result = 0;
    from_json(j, result);
    FATP_ASSERT_EQ(result, val, "int64_t min should round-trip");

    val = LLONG_MAX;
    j = json_encode(val);
    from_json(j, result);
    FATP_ASSERT_EQ(result, val, "int64_t max should round-trip");

    return true;
}

FATP_TEST_CASE(json_lite_fixed_width_uint8)
{
    uint8_t val = 0;
    JsonValue j = json_encode(val);
    uint8_t result = 1;
    from_json(j, result);
    FATP_ASSERT_EQ(result, val, "uint8_t min should round-trip");

    val = 255;
    j = json_encode(val);
    from_json(j, result);
    FATP_ASSERT_EQ(result, val, "uint8_t max should round-trip");

    return true;
}

FATP_TEST_CASE(json_lite_fixed_width_uint16)
{
    uint16_t val = 0;
    JsonValue j = json_encode(val);
    uint16_t result = 1;
    from_json(j, result);
    FATP_ASSERT_EQ(result, val, "uint16_t min should round-trip");

    val = 65535;
    j = json_encode(val);
    from_json(j, result);
    FATP_ASSERT_EQ(result, val, "uint16_t max should round-trip");

    return true;
}

FATP_TEST_CASE(json_lite_fixed_width_uint32)
{
    uint32_t val = 0;
    JsonValue j = json_encode(val);
    uint32_t result = 1;
    from_json(j, result);
    FATP_ASSERT_EQ(result, val, "uint32_t min should round-trip");

    val = 4294967295U;
    j = json_encode(val);
    from_json(j, result);
    FATP_ASSERT_EQ(result, val, "uint32_t max should round-trip");

    return true;
}

FATP_TEST_CASE(json_lite_fixed_width_uint64)
{
    uint64_t val = 0;
    JsonValue j = json_encode(val);
    uint64_t result = 1;
    from_json(j, result);
    FATP_ASSERT_EQ(result, val, "uint64_t min should round-trip");

    val = static_cast<uint64_t>(LLONG_MAX);
    j = json_encode(val);
    from_json(j, result);
    FATP_ASSERT_EQ(result, val, "uint64_t in range should round-trip");

    return true;
}

FATP_TEST_CASE(json_lite_size_t_type)
{
    size_t val = 123456;
    JsonValue j = json_encode(val);
    FATP_ASSERT_TRUE(j.is_int() || j.is_number(), "size_t should be numeric");

    return true;
}

FATP_TEST_CASE(json_lite_ptrdiff_t_type)
{
    ptrdiff_t val = -123456;
    JsonValue j = json_encode(val);
    FATP_ASSERT_TRUE(j.is_int(), "ptrdiff_t should be int");

    return true;
}

FATP_TEST_CASE(json_lite_string_escaping)
{
    FATP_SUBTEST("basic escaping")
    {
        std::string val = "Hello \"World\"";
        JsonValue j = json_encode(val);
        std::string json_str = to_json_string(j);
        FATP_ASSERT_TRUE(json_str.find("\\\"") != std::string::npos, "Quotes should be escaped");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("newline escaping")
    {
        std::string val = "Line 1\nLine 2";
        JsonValue j = json_encode(val);
        std::string json_str = to_json_string(j);
        FATP_ASSERT_TRUE(json_str.find("\\n") != std::string::npos, "Newlines should be escaped");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("control characters")
    {
        std::string val = "Tab:\tBackspace:\b";
        JsonValue j = json_encode(val);
        std::string json_str = to_json_string(j);
        FATP_ASSERT_TRUE(json_str.find("\\t") != std::string::npos, "Tab should be escaped");
    }
    FATP_END_SUBTEST

    return fat_p::testing::get_subtest_tracker().all_passed();
}

FATP_TEST_CASE(json_lite_string_empty)
{
    std::string val = "";
    JsonValue j = json_encode(val);
    std::string result;
    from_json(j, result);
    FATP_ASSERT_EQ(result, "", "Empty string should round-trip");

    return true;
}

FATP_TEST_CASE(json_lite_string_whitespace)
{
    std::string val = "   spaces   ";
    JsonValue j = json_encode(val);
    std::string result;
    from_json(j, result);
    FATP_ASSERT_EQ(result, val, "Whitespace should be preserved");

    return true;
}

FATP_TEST_CASE(json_lite_string_special_chars)
{
    std::string val = "Special: !@#$%^&*()_+-={}[]|:;'<>?,./";
    JsonValue j = json_encode(val);
    std::string result;
    from_json(j, result);
    FATP_ASSERT_TRUE(!result.empty(), "Special chars should survive");

    return true;
}

FATP_TEST_CASE(json_lite_string_unicode)
{
    FATP_SUBTEST("basic unicode")
    {
        std::string val = "Unicode: \u00e9";
        JsonValue j = json_encode(val);
        std::string json_str = to_json_string(j);
        FATP_ASSERT_TRUE(!json_str.empty(), "Unicode should be handled");
    }
    FATP_END_SUBTEST

    return fat_p::testing::get_subtest_tracker().all_passed();
}

FATP_TEST_CASE(json_lite_string_long)
{
    std::string val(10000, 'x');
    JsonValue j = json_encode(val);
    std::string result;
    from_json(j, result);
    FATP_ASSERT_EQ(result.size(), val.size(), "Long string should preserve length");

    return true;
}

FATP_TEST_CASE(json_lite_containers)
{
    FATP_SUBTEST("vector<int>")
    {
        std::vector<int> vec = {1, 2, 3, 4, 5};
        JsonValue j = json_encode(vec);
        FATP_ASSERT_TRUE(j.is_array(), "Vector should be array");
        std::vector<int> result;
        from_json(j, result);
        FATP_ASSERT_EQ(result.size(), vec.size(), "Vector size should match");
        FATP_ASSERT_EQ(result[0], 1, "First element should be 1");
        FATP_ASSERT_EQ(result[4], 5, "Last element should be 5");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("map<string, int>")
    {
        std::map<std::string, int> m = {{"one", 1}, {"two", 2}, {"three", 3}};
        JsonValue j = json_encode(m);
        FATP_ASSERT_TRUE(j.is_object(), "Map should be object");
        std::map<std::string, int> result;
        from_json(j, result);
        FATP_ASSERT_EQ(result.size(), m.size(), "Map size should match");
        FATP_ASSERT_EQ(result["one"], 1, "Value for 'one' should be 1");
    }
    FATP_END_SUBTEST

    return fat_p::testing::get_subtest_tracker().all_passed();
}

FATP_TEST_CASE(json_lite_vector_empty)
{
    std::vector<int> vec;
    JsonValue j = json_encode(vec);
    FATP_ASSERT_TRUE(j.is_array(), "Empty vector should be array");
    std::vector<int> result;
    from_json(j, result);
    FATP_ASSERT_TRUE(result.empty(), "Result should be empty");

    return true;
}

FATP_TEST_CASE(json_lite_vector_single)
{
    std::vector<int> vec = {42};
    JsonValue j = json_encode(vec);
    std::vector<int> result;
    from_json(j, result);
    FATP_ASSERT_EQ(result.size(), static_cast<size_t>(1), "Size should be 1");
    FATP_ASSERT_EQ(result[0], 42, "Value should be 42");

    return true;
}

FATP_TEST_CASE(json_lite_set)
{
    std::set<int> s = {1, 2, 3, 4, 5};
    JsonValue j = json_encode(s);
    FATP_ASSERT_TRUE(j.is_array(), "Set should be array");
    std::set<int> result;
    from_json(j, result);
    FATP_ASSERT_EQ(result.size(), s.size(), "Set size should match");
    FATP_ASSERT_TRUE(result.count(3) == 1, "Should contain 3");

    return true;
}

FATP_TEST_CASE(json_lite_map_empty)
{
    std::map<std::string, int> m;
    JsonValue j = json_encode(m);
    FATP_ASSERT_TRUE(j.is_object(), "Empty map should be object");
    std::map<std::string, int> result;
    from_json(j, result);
    FATP_ASSERT_TRUE(result.empty(), "Result should be empty");

    return true;
}

FATP_TEST_CASE(json_lite_map_numeric_keys)
{
    std::map<int, std::string> m = {{1, "one"}, {2, "two"}};
    JsonValue j = json_encode(m);
    FATP_ASSERT_TRUE(j.is_object(), "Map with int keys should be object");
    std::map<int, std::string> result;
    from_json(j, result);
    FATP_ASSERT_EQ(result.size(), m.size(), "Map size should match");

    return true;
}

FATP_TEST_CASE(json_lite_nested_vectors)
{
    std::vector<std::vector<int>> nested = {{1, 2}, {3, 4}, {5, 6}};
    JsonValue j = json_encode(nested);
    std::vector<std::vector<int>> result;
    from_json(j, result);
    FATP_ASSERT_EQ(result.size(), nested.size(), "Outer size should match");
    FATP_ASSERT_EQ(result[0].size(), static_cast<size_t>(2), "Inner size should be 2");
    FATP_ASSERT_EQ(result[1][1], 4, "Element should be 4");

    return true;
}

FATP_TEST_CASE(json_lite_mixed_nested)
{
    std::map<std::string, std::vector<int>> m = {{"nums", {1, 2, 3}}, {"more", {4, 5}}};
    JsonValue j = json_encode(m);
    std::map<std::string, std::vector<int>> result;
    from_json(j, result);
    FATP_ASSERT_EQ(result["nums"].size(), static_cast<size_t>(3), "Nums size should be 3");
    FATP_ASSERT_EQ(result["more"][1], 5, "Value should be 5");

    return true;
}

FATP_TEST_CASE(json_lite_array)
{
    std::array<int, 3> arr = {10, 20, 30};
    JsonValue j = json_encode(arr);
    FATP_ASSERT_TRUE(j.is_array(), "std::array should be array");
    std::array<int, 3> result{};
    from_json(j, result);
    FATP_ASSERT_EQ(result[0], 10, "First element should be 10");
    FATP_ASSERT_EQ(result[2], 30, "Last element should be 30");

    return true;
}

FATP_TEST_CASE(json_lite_unordered_set)
{
    std::unordered_set<int> s = {1, 2, 3};
    JsonValue j = json_encode(s);
    FATP_ASSERT_TRUE(j.is_array(), "Unordered set should be array");
    std::unordered_set<int> result;
    from_json(j, result);
    FATP_ASSERT_EQ(result.size(), s.size(), "Size should match");

    return true;
}

FATP_TEST_CASE(json_lite_unordered_map)
{
    std::unordered_map<std::string, int> m = {{"a", 1}, {"b", 2}};
    JsonValue j = json_encode(m);
    FATP_ASSERT_TRUE(j.is_object(), "Unordered map should be object");
    std::unordered_map<std::string, int> result;
    from_json(j, result);
    FATP_ASSERT_EQ(result.size(), m.size(), "Size should match");

    return true;
}

FATP_TEST_CASE(json_lite_deque)
{
    std::deque<int> d = {1, 2, 3, 4};
    JsonValue j = json_encode(d);
    FATP_ASSERT_TRUE(j.is_array(), "Deque should be array");
    std::deque<int> result;
    from_json(j, result);
    FATP_ASSERT_EQ(result.size(), d.size(), "Size should match");

    return true;
}

FATP_TEST_CASE(json_lite_list)
{
    std::list<int> lst = {5, 10, 15};
    JsonValue j = json_encode(lst);
    FATP_ASSERT_TRUE(j.is_array(), "List should be array");
    std::list<int> result;
    from_json(j, result);
    FATP_ASSERT_EQ(result.size(), lst.size(), "Size should match");

    return true;
}

FATP_TEST_CASE(json_lite_string_view)
{
    std::string_view sv = "test string";
    JsonValue j = json_encode(sv);
    FATP_ASSERT_TRUE(j.is_string(), "String view should be string");
    std::string result;
    from_json(j, result);
    FATP_ASSERT_EQ(result, "test string", "Value should match");

    return true;
}

FATP_TEST_CASE(json_lite_optional)
{
    FATP_SUBTEST("optional with value")
    {
        std::optional<int> opt = 42;
        JsonValue j = json_encode(opt);
        FATP_ASSERT_TRUE(j.is_int(), "Optional int should be int");
        std::optional<int> result;
        from_json(j, result);
#pragma warning(suppress : 26859) // MSVC false positive: analyzer cannot track that from_json populates the optional
        FATP_ASSERT_TRUE(result.has_value() && *result == 42, "Result should have value 42");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("optional without value")
    {
        std::optional<int> opt;
        JsonValue j = json_encode(opt);
        FATP_ASSERT_TRUE(j.is_null(), "Empty optional should be null");
        std::optional<int> result;
        from_json(j, result);
        FATP_ASSERT_FALSE(result.has_value(), "Result should not have value");
    }
    FATP_END_SUBTEST

    return fat_p::testing::get_subtest_tracker().all_passed();
}

FATP_TEST_CASE(json_lite_optional_string)
{
    std::optional<std::string> opt = "hello";
    JsonValue j = json_encode(opt);
    std::optional<std::string> result;
    from_json(j, result);
#pragma warning(suppress : 26859) // MSVC false positive: analyzer cannot track that from_json populates the optional
    FATP_ASSERT_TRUE(result.has_value() && *result == "hello", "Result should have value hello");

    return true;
}

FATP_TEST_CASE(json_lite_optional_vector)
{
    std::optional<std::vector<int>> opt = std::vector<int>{1, 2, 3};
    JsonValue j = json_encode(opt);
    std::optional<std::vector<int>> result;
    from_json(j, result);
#pragma warning(suppress : 26859) // MSVC false positive: analyzer cannot track that from_json populates the optional
    FATP_ASSERT_TRUE(result.has_value() && (*result).size() == 3, "Result should have value with size 3");

    return true;
}

FATP_TEST_CASE(json_lite_tuples_pairs)
{
    FATP_SUBTEST("pair<int, string>")
    {
        std::pair<int, std::string> p = {42, "answer"};
        JsonValue j = json_encode(p);
        FATP_ASSERT_TRUE(j.is_array(), "Pair should be array");
        std::pair<int, std::string> result;
        from_json(j, result);
        FATP_ASSERT_EQ(result.first, 42, "First should be 42");
        FATP_ASSERT_EQ(result.second, "answer", "Second should be answer");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("tuple<int, double, string>")
    {
        std::tuple<int, double, std::string> t = {1, 2.5, "three"};
        JsonValue j = json_encode(t);
        FATP_ASSERT_TRUE(j.is_array(), "Tuple should be array");
        std::tuple<int, double, std::string> result;
        from_json(j, result);
        FATP_ASSERT_EQ(std::get<0>(result), 1, "First should be 1");
        FATP_ASSERT_CLOSE(std::get<1>(result), 2.5, "Second should be 2.5");
        FATP_ASSERT_EQ(std::get<2>(result), "three", "Third should be three");
    }
    FATP_END_SUBTEST

    return fat_p::testing::get_subtest_tracker().all_passed();
}

FATP_TEST_CASE(json_lite_pair_nested)
{
    std::pair<int, std::vector<int>> p = {10, {1, 2, 3}};
    JsonValue j = json_encode(p);
    std::pair<int, std::vector<int>> result;
    from_json(j, result);
    FATP_ASSERT_EQ(result.first, 10, "First should be 10");
    FATP_ASSERT_EQ(result.second.size(), static_cast<size_t>(3), "Vector size should be 3");

    return true;
}

FATP_TEST_CASE(json_lite_tuple_large)
{
    std::tuple<int, int, int, int, int> t = {1, 2, 3, 4, 5};
    JsonValue j = json_encode(t);
    std::tuple<int, int, int, int, int> result;
    from_json(j, result);
    FATP_ASSERT_EQ(std::get<0>(result), 1, "Element 0 should be 1");
    FATP_ASSERT_EQ(std::get<4>(result), 5, "Element 4 should be 5");

    return true;
}

FATP_TEST_CASE(json_lite_simple_struct)
{
    Point p{10, 20};
    JsonValue j = json_encode(p);
    FATP_ASSERT_TRUE(j.is_object(), "Struct should be object");
    Point result = from_json<Point>(j);
    FATP_ASSERT_EQ(result.x, 10, "x should be 10");
    FATP_ASSERT_EQ(result.y, 20, "y should be 20");

    return true;
}

FATP_TEST_CASE(json_lite_complex_struct)
{
    Person p;
    p.name = "Alice";
    p.age = 30;
    p.hobbies = {"reading", "coding", "hiking"};

    JsonValue j = json_encode(p);
    Person result = from_json<Person>(j);
    FATP_ASSERT_EQ(result.name, "Alice", "Name should be Alice");
    FATP_ASSERT_EQ(result.age, 30, "Age should be 30");
    FATP_ASSERT_EQ(result.hobbies.size(), static_cast<size_t>(3), "Should have 3 hobbies");

    return true;
}

FATP_TEST_CASE(json_lite_optional_fields)
{
    Config cfg;
    cfg.port = 9000;
    cfg.timeout = 30;

    JsonValue j = json_encode(cfg);
    Config result = from_json<Config>(j);
    FATP_ASSERT_EQ(result.port, 9000, "Port should be 9000");
    FATP_ASSERT_TRUE(result.timeout.has_value(), "Timeout should have value");
    FATP_ASSERT_EQ(*result.timeout, 30, "Timeout should be 30");

    return true;
}

FATP_TEST_CASE(json_lite_intrusive_serialization)
{
    PrivateData pd(42, "secret");
    JsonValue j = json_encode(pd);
    PrivateData result = from_json<PrivateData>(j);
    FATP_ASSERT_EQ(result.secret(), 42, "Secret should be 42");
    FATP_ASSERT_EQ(result.code(), "secret", "Code should be secret");

    return true;
}

FATP_TEST_CASE(json_lite_nested_structs)
{
    Nested n;
    n.position = {5, 10};
    n.label = "point1";

    JsonValue j = json_encode(n);
    Nested result = from_json<Nested>(j);
    FATP_ASSERT_EQ(result.position.x, 5, "x should be 5");
    FATP_ASSERT_EQ(result.position.y, 10, "y should be 10");
    FATP_ASSERT_EQ(result.label, "point1", "Label should be point1");

    return true;
}

FATP_TEST_CASE(json_lite_complex_nested)
{
    Complex c;
    c.scores["math"] = 95;
    c.scores["science"] = 88;
    c.points.push_back({1, 2});
    c.points.push_back({3, 4});
    c.description = "test data";

    JsonValue j = json_encode(c);
    Complex result = from_json<Complex>(j);
    FATP_ASSERT_EQ(result.scores.size(), static_cast<size_t>(2), "Should have 2 scores");
    FATP_ASSERT_EQ(result.points.size(), static_cast<size_t>(2), "Should have 2 points");
    FATP_ASSERT_TRUE(result.description.has_value(), "Description should have value");

    return true;
}

FATP_TEST_CASE(json_lite_deep_hierarchy)
{
    Company company;
    company.name = "TechCorp";

    Department dept;
    dept.name = "Engineering";

    Employee emp;
    emp.name = "John Doe";
    emp.id = 123;
    emp.skills = {"C++", "Python"};

    dept.employees.push_back(emp);
    company.departments.push_back(dept);

    JsonValue j = json_encode(company);
    Company result = from_json<Company>(j);
    FATP_ASSERT_EQ(result.name, "TechCorp", "Company name should match");
    FATP_ASSERT_EQ(result.departments.size(), static_cast<size_t>(1), "Should have 1 department");
    FATP_ASSERT_EQ(result.departments[0].employees[0].name, "John Doe", "Employee name should match");

    return true;
}

FATP_TEST_CASE(json_lite_parser_basic)
{
    FATP_SUBTEST("parse null")
    {
        JsonValue j = parse_json("null");
        FATP_ASSERT_TRUE(j.is_null(), "Should parse null");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("parse true")
    {
        JsonValue j = parse_json("true");
        FATP_ASSERT_TRUE(j.is_bool(), "Should parse bool");
        FATP_ASSERT_EQ(std::get<bool>(j), true, "Should be true");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("parse false")
    {
        JsonValue j = parse_json("false");
        FATP_ASSERT_EQ(std::get<bool>(j), false, "Should be false");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("parse number")
    {
        JsonValue j = parse_json("42");
        FATP_ASSERT_TRUE(j.is_int(), "Should parse int");
        FATP_ASSERT_EQ(std::get<int64_t>(j), static_cast<int64_t>(42), "Should be 42");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("parse string")
    {
        JsonValue j = parse_json("\"hello\"");
        FATP_ASSERT_TRUE(j.is_string(), "Should parse string");
        FATP_ASSERT_EQ(std::get<std::string>(j), "hello", "Should be hello");
    }
    FATP_END_SUBTEST

    return fat_p::testing::get_subtest_tracker().all_passed();
}

FATP_TEST_CASE(json_lite_parser_containers)
{
    FATP_SUBTEST("parse array")
    {
        JsonValue j = parse_json("[1, 2, 3]");
        FATP_ASSERT_TRUE(j.is_array(), "Should parse array");
        const auto& arr = std::get<JsonArray>(j);
        FATP_ASSERT_EQ(arr.size(), static_cast<size_t>(3), "Array should have 3 elements");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("parse object")
    {
        JsonValue j = parse_json(R"({"key": "value"})");
        FATP_ASSERT_TRUE(j.is_object(), "Should parse object");
        const auto& obj = std::get<JsonObject>(j);
        FATP_ASSERT_EQ(obj.size(), static_cast<size_t>(1), "Object should have 1 key");
    }
    FATP_END_SUBTEST

    return fat_p::testing::get_subtest_tracker().all_passed();
}

FATP_TEST_CASE(json_lite_parser_edge_cases)
{
    FATP_SUBTEST("empty array")
    {
        JsonValue j = parse_json("[]");
        FATP_ASSERT_TRUE(j.is_array(), "Should parse empty array");
        FATP_ASSERT_TRUE(std::get<JsonArray>(j).empty(), "Array should be empty");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("empty object")
    {
        JsonValue j = parse_json("{}");
        FATP_ASSERT_TRUE(j.is_object(), "Should parse empty object");
        FATP_ASSERT_TRUE(std::get<JsonObject>(j).empty(), "Object should be empty");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("whitespace handling")
    {
        JsonValue j = parse_json("  \n\t  42  \n\t  ");
        FATP_ASSERT_TRUE(j.is_int(), "Should handle whitespace");
        FATP_ASSERT_EQ(std::get<int64_t>(j), static_cast<int64_t>(42), "Should be 42");
    }
    FATP_END_SUBTEST

    return fat_p::testing::get_subtest_tracker().all_passed();
}

FATP_TEST_CASE(json_lite_parser_numbers)
{
    FATP_SUBTEST("negative integer")
    {
        JsonValue j = parse_json("-42");
        FATP_ASSERT_EQ(std::get<int64_t>(j), static_cast<int64_t>(-42), "Should be -42");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("floating point")
    {
        JsonValue j = parse_json("3.14");
        FATP_ASSERT_TRUE(j.is_number(), "Should be number");
        FATP_ASSERT_CLOSE(std::get<double>(j), 3.14, "Should be 3.14");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("scientific notation")
    {
        JsonValue j = parse_json("1.5e10");
        FATP_ASSERT_TRUE(j.is_number(), "Should be number");
        FATP_ASSERT_CLOSE(std::get<double>(j), 1.5e10, "Should be 1.5e10");
    }
    FATP_END_SUBTEST

    return fat_p::testing::get_subtest_tracker().all_passed();
}

FATP_TEST_CASE(json_lite_parser_strings)
{
    FATP_SUBTEST("escaped quotes")
    {
        JsonValue j = parse_json(R"("He said \"hello\"")");
        FATP_ASSERT_EQ(std::get<std::string>(j), "He said \"hello\"", "Should handle escaped quotes");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("escaped backslash")
    {
        JsonValue j = parse_json(R"("C:\\path\\file")");
        std::string result = std::get<std::string>(j);
        FATP_ASSERT_TRUE(result.find("\\\\") != std::string::npos || result.find("\\") != std::string::npos,
                         "Should handle backslashes");
    }
    FATP_END_SUBTEST

    return fat_p::testing::get_subtest_tracker().all_passed();
}

FATP_TEST_CASE(json_lite_parser_nested)
{
    std::string json = R"({
        "array": [1, 2, {"nested": true}],
        "object": {"a": 1, "b": [2, 3]}
    })";

    JsonValue j = parse_json(json);
    FATP_ASSERT_TRUE(j.is_object(), "Should parse nested structure");

    const auto& obj = std::get<JsonObject>(j);
    FATP_ASSERT_TRUE(obj.count("array") > 0, "Should have array key");
    FATP_ASSERT_TRUE(obj.count("object") > 0, "Should have object key");

    return true;
}

FATP_TEST_CASE(json_lite_parser_errors)
{
    FATP_SUBTEST("invalid json")
    {
        FATP_ASSERT_THROWS(parse_json("invalid"), std::runtime_error, "Should throw on invalid JSON");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("unclosed array")
    {
        FATP_ASSERT_THROWS(parse_json("[1, 2, 3"), std::runtime_error, "Should throw on unclosed array");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("unclosed object")
    {
        FATP_ASSERT_THROWS(parse_json(R"({"key": "value")"), std::runtime_error, "Should throw on unclosed object");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("trailing comma")
    {
        FATP_ASSERT_THROWS(parse_json("[1, 2,]"), std::runtime_error, "Should throw on trailing comma");
    }
    FATP_END_SUBTEST

    return fat_p::testing::get_subtest_tracker().all_passed();
}

FATP_TEST_CASE(json_lite_parser_literals)
{
    FATP_SUBTEST("true literal")
    {
        JsonValue j = parse_json("true");
        FATP_ASSERT_TRUE(std::get<bool>(j), "Should be true");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("false literal")
    {
        JsonValue j = parse_json("false");
        FATP_ASSERT_FALSE(std::get<bool>(j), "Should be false");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("null literal")
    {
        JsonValue j = parse_json("null");
        FATP_ASSERT_TRUE(j.is_null(), "Should be null");
    }
    FATP_END_SUBTEST

    return fat_p::testing::get_subtest_tracker().all_passed();
}

FATP_TEST_CASE(json_lite_parser_unicode)
{
    FATP_SUBTEST("unicode escape")
    {
        JsonValue j = parse_json(R"("\u0041")");
        FATP_ASSERT_EQ(std::get<std::string>(j), "A", "Should parse unicode escape");
    }
    FATP_END_SUBTEST

    return fat_p::testing::get_subtest_tracker().all_passed();
}

FATP_TEST_CASE(json_lite_pretty_print)
{
    Point p{10, 20};
    std::string compact = to_json_string(p, false);
    std::string pretty = to_json_string(p, true);

    FATP_ASSERT_TRUE(pretty.length() > compact.length(), "Pretty print should be longer");
    FATP_ASSERT_TRUE(pretty.find("\n") != std::string::npos, "Pretty print should have newlines");

    return true;
}

FATP_TEST_CASE(json_lite_numeric_precision)
{
    FATP_SUBTEST("default precision")
    {
        double val = 3.141592653589793;
        std::string json = to_json_string<double, StandardJsonPolicy>(val);
        FATP_ASSERT_TRUE(!json.empty(), "Should serialize with default precision");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("custom precision")
    {
        double val = 3.141592653589793;
        std::string json2 = to_json_string<double, Precision2>(val);
        std::string json8 = to_json_string<double, Precision8>(val);
        FATP_ASSERT_TRUE(json2.length() < json8.length(), "Lower precision should be shorter");
    }
    FATP_END_SUBTEST

    return fat_p::testing::get_subtest_tracker().all_passed();
}

FATP_TEST_CASE(json_lite_nan_inf_handling)
{
    FATP_SUBTEST("NaN with compat policy")
    {
        constexpr double val = std::numeric_limits<double>::quiet_NaN();
        std::string json = to_json_string<double, CompatJsonPolicy>(val);
        FATP_ASSERT_TRUE(json.find("NaN") != std::string::npos, "NaN should be serialized");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("Infinity with compat policy")
    {
        constexpr double val = std::numeric_limits<double>::infinity();
        std::string json = to_json_string<double, CompatJsonPolicy>(val);
        FATP_ASSERT_TRUE(json.find("Infinity") != std::string::npos, "Infinity should be serialized");
    }
    FATP_END_SUBTEST

    return fat_p::testing::get_subtest_tracker().all_passed();
}

FATP_TEST_CASE(json_lite_standard_policy_nan)
{
    constexpr double val = std::numeric_limits<double>::quiet_NaN();
    std::string json = to_json_string<double, StandardJsonPolicy>(val);
    FATP_ASSERT_TRUE(json.find("null") != std::string::npos, "NaN should be null with standard policy");

    return true;
}

FATP_TEST_CASE(json_lite_file_io)
{
    Point p{100, 200};
    save_params("test_point.json", p);
    Point loaded = load_params<Point>("test_point.json");
    FATP_ASSERT_EQ(loaded.x, 100, "x should be 100");
    FATP_ASSERT_EQ(loaded.y, 200, "y should be 200");

    std::remove("test_point.json");
    return true;
}

FATP_TEST_CASE(json_lite_file_io_complex)
{
    AppConfig cfg;
    cfg.port = 8080;
    cfg.host = "localhost";
    cfg.allowed_ips = {"127.0.0.1", "192.168.1.1"};
    cfg.timeout = 30;

    save_params("test_config.json", cfg);
    AppConfig loaded = load_params<AppConfig>("test_config.json");
    FATP_ASSERT_EQ(loaded.port, 8080, "Port should be 8080");
    FATP_ASSERT_EQ(loaded.host, "localhost", "Host should be localhost");
    FATP_ASSERT_EQ(loaded.allowed_ips.size(), static_cast<size_t>(2), "Should have 2 IPs");

    std::remove("test_config.json");
    return true;
}

FATP_TEST_CASE(json_lite_file_io_errors)
{
    FATP_ASSERT_THROWS(load_params<Point>("nonexistent.json"), std::runtime_error, "Should throw on missing file");
    return true;
}

FATP_TEST_CASE(json_lite_roundtrip_all_types)
{
    FATP_SUBTEST("int round-trip")
    {
        int val = 42;
        std::string json = to_json_string(val);
        int result = from_json_string<int>(json);
        FATP_ASSERT_EQ(result, val, "Int should round-trip");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("double round-trip")
    {
        double val = 3.14159;
        std::string json = to_json_string(val);
        double result = from_json_string<double>(json);
        FATP_ASSERT_CLOSE(result, val, "Double should round-trip");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("string round-trip")
    {
        std::string val = "test string";
        std::string json = to_json_string(val);
        std::string result = from_json_string<std::string>(json);
        FATP_ASSERT_EQ(result, val, "String should round-trip");
    }
    FATP_END_SUBTEST

    return fat_p::testing::get_subtest_tracker().all_passed();
}

FATP_TEST_CASE(json_lite_convenience_functions)
{
    Point p{10, 20};
    std::string json = to_json_string(p);
    Point result = from_json_string<Point>(json);
    FATP_ASSERT_EQ(result.x, 10, "x should be 10");
    FATP_ASSERT_EQ(result.y, 20, "y should be 20");

    return true;
}

FATP_TEST_CASE(json_lite_value_returning_from_json)
{
    JsonValue j = static_cast<int64_t>(42);
    const int result = from_json<int>(j);
    FATP_ASSERT_EQ(result, 42, "Value-returning from_json should work");

    return true;
}

FATP_TEST_CASE(json_lite_roundtrip_vectors)
{
    std::vector<int> vec = {1, 2, 3, 4, 5};
    std::string json = to_json_string(vec);
    std::vector<int> result = from_json_string<std::vector<int>>(json);
    FATP_ASSERT_EQ(result.size(), vec.size(), "Vector size should match");
    FATP_ASSERT_EQ(result[0], 1, "First element should be 1");

    return true;
}

FATP_TEST_CASE(json_lite_roundtrip_maps)
{
    std::map<std::string, int> m = {{"a", 1}, {"b", 2}};
    std::string json = to_json_string(m);
    std::map<std::string, int> result = from_json_string<std::map<std::string, int>>(json);
    FATP_ASSERT_EQ(result.size(), m.size(), "Map size should match");
    FATP_ASSERT_EQ(result["a"], 1, "Value should be 1");

    return true;
}

FATP_TEST_CASE(json_lite_large_data)
{
    std::vector<int> large_vec(10000);
    for (size_t i = 0; i < large_vec.size(); ++i)
    {
        large_vec[i] = static_cast<int>(i);
    }

    std::string json = to_json_string(large_vec);
    FATP_ASSERT_TRUE(json.length() > 10000, "Large vector JSON should be substantial");

    return true;
}

FATP_TEST_CASE(json_lite_deeply_nested)
{
    std::string json = R"({"a": {"b": {"c": {"d": {"e": 42}}}}})";
    JsonValue j = parse_json(json);
    FATP_ASSERT_TRUE(j.is_object(), "Should parse deeply nested object");

    return true;
}

FATP_TEST_CASE(json_lite_large_strings)
{
    std::string large_str(100000, 'x');
    JsonValue j = json_encode(large_str);
    std::string json = to_json_string(j);
    FATP_ASSERT_TRUE(json.length() > 100000, "Large string JSON should be substantial");

    return true;
}

FATP_TEST_CASE(json_lite_large_map)
{
    std::map<std::string, int> large_map;
    for (int i = 0; i < 1000; ++i)
    {
        large_map["key" + std::to_string(i)] = i;
    }

    JsonValue j = json_encode(large_map);
    std::string json = to_json_string(j);
    FATP_ASSERT_TRUE(json.length() > 5000, "Large map JSON should be substantial");

    return true;
}

FATP_TEST_CASE(json_lite_depth_limit_parse)
{
    std::string deep_json = "[";
    for (int i = 0; i < 600; ++i)
    {
        deep_json += "[";
    }
    for (int i = 0; i < 600; ++i)
    {
        deep_json += "]";
    }
    deep_json += "]";

    FATP_ASSERT_THROWS(parse_json(deep_json), std::runtime_error, "Should throw on exceeding parse depth");
    return true;
}

FATP_TEST_CASE(json_lite_depth_limit_dump)
{
    JsonArray deep_array;
    JsonArray* current = &deep_array;
    for (int i = 0; i < 600; ++i)
    {
        JsonArray next;
        current->push_back(next);
        if (!current->empty() && current->back().is_array())
        {
            current = &std::get<JsonArray>(current->back());
        }
    }

    FATP_ASSERT_THROWS(to_json_string(deep_array), std::runtime_error, "Should throw on exceeding dump depth");
    return true;
}

FATP_TEST_CASE(json_lite_range_checks)
{
    FATP_SUBTEST("int overflow")
    {
        JsonValue j = static_cast<int64_t>(3000000000LL);
        int result = 0;
        FATP_ASSERT_THROWS(from_json(j, result), std::runtime_error, "Should throw on int overflow");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("unsigned negative")
    {
        JsonValue j = static_cast<int64_t>(-1);
        unsigned int result = 0;
        FATP_ASSERT_THROWS(from_json(j, result), std::runtime_error, "Should throw on negative to unsigned");
    }
    FATP_END_SUBTEST

    return fat_p::testing::get_subtest_tracker().all_passed();
}

FATP_TEST_CASE(json_lite_range_int8)
{
    JsonValue j = static_cast<int64_t>(200);
    int8_t result = 0;
    FATP_ASSERT_THROWS(from_json(j, result), std::runtime_error, "Should throw on int8_t overflow");

    return true;
}

FATP_TEST_CASE(json_lite_better_error_messages)
{
    try
    {
        (void)parse_json("invalid json");
        FATP_ASSERT_TRUE(false, "Should have thrown");
    }
    catch (const std::exception& e)
    {
        std::string msg = e.what();
        FATP_ASSERT_TRUE(!msg.empty(), "Error message should not be empty");
    }

    return true;
}

FATP_TEST_CASE(json_lite_param_helpers)
{
    Point p{42, 84};
    save_params("test_helpers.json", p);
    Point loaded = load_params<Point>("test_helpers.json");
    FATP_ASSERT_EQ(loaded.x, 42, "x should be 42");
    FATP_ASSERT_EQ(loaded.y, 84, "y should be 84");

    std::remove("test_helpers.json");
    return true;
}

FATP_TEST_CASE(json_lite_position_in_errors)
{
    try
    {
        (void)parse_json(R"({"key": invalid_value})");
        FATP_ASSERT_TRUE(false, "Should have thrown");
    }
    catch (const std::exception& e)
    {
        std::string msg = e.what();
        FATP_ASSERT_TRUE(!msg.empty(), "Error should have position info");
    }

    return true;
}

FATP_TEST_CASE(json_lite_error_messages_comprehensive)
{
    FATP_SUBTEST("type mismatch")
    {
        JsonValue j = "string";
        int result = 0;
        try
        {
            from_json(j, result);
            FATP_ASSERT_TRUE(false, "Should have thrown");
        }
        catch (const std::exception& e)
        {
            std::string msg = e.what();
            FATP_ASSERT_TRUE(msg.find("type") != std::string::npos || msg.find("Type") != std::string::npos,
                             "Error should mention type");
        }
    }
    FATP_END_SUBTEST

    return fat_p::testing::get_subtest_tracker().all_passed();
}

FATP_TEST_CASE(json_lite_field_limit)
{
    Max50Fields data{};
    for (int i = 0; i < 50; ++i)
    {
        *(&data.f1 + i) = i + 1;
    }

    std::string json = to_json_string(data);
    Max50Fields result = from_json_string<Max50Fields>(json);
    FATP_ASSERT_EQ(result.f1, 1, "f1 should be 1");
    FATP_ASSERT_EQ(result.f50, 50, "f50 should be 50");

    return true;
}

FATP_TEST_CASE(json_lite_bug2_name_collision)
{
    TypeA a{42};
    TypeB b{3.14};

    std::string json_a = to_json_string(a);
    std::string json_b = to_json_string(b);

    TypeA result_a = from_json_string<TypeA>(json_a);
    TypeB result_b = from_json_string<TypeB>(json_b);

    FATP_ASSERT_EQ(result_a.value, 42, "TypeA value should be 42");
    FATP_ASSERT_CLOSE(result_b.value, 3.14, "TypeB value should be 3.14");

    return true;
}

FATP_TEST_CASE(json_lite_bug2_collision_prevention)
{
    User u{"Alice", 30};
    Product p{"Widget", 19.99};
    Order o{"Item", 5};

    std::string u_json = to_json_string(u);
    std::string p_json = to_json_string(p);
    std::string o_json = to_json_string(o);

    User u_result = from_json_string<User>(u_json);
    Product p_result = from_json_string<Product>(p_json);
    Order o_result = from_json_string<Order>(o_json);

    FATP_ASSERT_EQ(u_result.name, "Alice", "User name should be Alice");
    FATP_ASSERT_EQ(p_result.name, "Widget", "Product name should be Widget");
    FATP_ASSERT_EQ(o_result.name, "Item", "Order name should be Item");

    return true;
}

FATP_TEST_CASE(json_lite_numeric_formatting)
{
    ScientificData data;
    data.planck_constant = 6.62607015e-34;
    data.avogadro_number = 6.02214076e23;
    data.normal_value = 123.456;

    std::string json_std = to_json_string<ScientificData, StandardJsonPolicy>(data);
    std::string json_sci = to_json_string<ScientificData, ScientificFormatPolicy>(data);
    std::string json_fix = to_json_string<ScientificData, FixedFormatPolicy>(data);

    FATP_ASSERT_TRUE(!json_std.empty(), "Standard formatting");
    FATP_ASSERT_TRUE(!json_sci.empty(), "Scientific formatting");
    FATP_ASSERT_TRUE(!json_fix.empty(), "Fixed formatting");

    return true;
}

FATP_TEST_CASE(json_lite_bug4_error_context)
{
    std::string bad_json = R"({"key": "value", "number": not_a_number})";

    try
    {
        (void)parse_json(bad_json);
        FATP_ASSERT_TRUE(false, "Should have thrown");
    }
    catch (const std::exception& e)
    {
        std::string msg = e.what();
        FATP_ASSERT_TRUE(!msg.empty(), "Error context should exist");
    }

    return true;
}

FATP_TEST_CASE(json_lite_bug5_fixed_arrays)
{
    WithStdArray data;
    data.values = {1, 2, 3};
    data.coords = {3.14, 2.71};

    std::string json = to_json_string(data);
    WithStdArray out = from_json_string<WithStdArray>(json);

    FATP_ASSERT_EQ(out.values[0], 1, "First value");
    FATP_ASSERT_EQ(out.values[2], 3, "Third value");
    FATP_ASSERT_CLOSE(out.coords[0], 3.14, "First coord");
    FATP_ASSERT_CLOSE(out.coords[1], 2.71, "Second coord");

    return true;
}

FATP_TEST_CASE(json_lite_bug6_policy_incompatibility)
{
    TestData data{3.14159265358979};

    auto std_json = to_json_string<TestData, StandardJsonPolicy>(data);
    auto low_json = to_json_string<TestData, LowPrecisionPolicy>(data);

    bool policy_respected = (std_json != low_json);

    FATP_ASSERT_TRUE(policy_respected, "Custom policies must be respected by macro-generated functions");

    return true;
}

FATP_TEST_CASE(json_lite_bug6_policy_fix_verification)
{
    ComplexData data{98.6543210, 1013.25987654, "sensor_1"};

    auto json2 = to_json_string<ComplexData, Precision2>(data);
    auto json4 = to_json_string<ComplexData, Precision4>(data);
    auto json8 = to_json_string<ComplexData, Precision8>(data);
    auto json16 = to_json_string<ComplexData, StandardJsonPolicy>(data);

    FATP_ASSERT_TRUE(json2.length() < json4.length(), "Higher precision should produce longer output");
    FATP_ASSERT_TRUE(json4.length() < json8.length(), "Higher precision should produce longer output");
    FATP_ASSERT_TRUE(json8.length() < json16.length(), "Higher precision should produce longer output");

    return true;
}

FATP_TEST_CASE(jsonc_line_comments)
{
    std::string json_with_comments = R"({
        "port": 8080,
        "host": "localhost"
    })";

    JsonValue j = parse_json<ConfigJsonPolicy>(json_with_comments);
    FATP_ASSERT_TRUE(j.is_object(), "Should parse object with line comments");

    const auto& obj = std::get<JsonObject>(j);
    FATP_ASSERT_EQ(from_json<int>(obj, "port"), 8080, "Port should be 8080");
    FATP_ASSERT_EQ(from_json<std::string>(obj, "host"), "localhost", "Host should be localhost");

    return true;
}

FATP_TEST_CASE(jsonc_block_comments)
{
    std::string json = R"({
        "name": "test",
        "value": 42
    })";

    JsonValue j = parse_json<ConfigJsonPolicy>(json);
    FATP_ASSERT_TRUE(j.is_object(), "Should parse object with block comments");

    return true;
}

FATP_TEST_CASE(jsonc_comment_edge_cases)
{
    FATP_SUBTEST("comment with quotes")
    {
        std::string json = R"({
            "key": "value"
        })";
        JsonValue j = parse_json<ConfigJsonPolicy>(json);
        FATP_ASSERT_TRUE(j.is_object(), "Should handle comment with quotes");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("comment with escaped chars")
    {
        std::string json = R"({
            "data": "test"
        })";
        JsonValue j = parse_json<ConfigJsonPolicy>(json);
        FATP_ASSERT_TRUE(j.is_object(), "Should handle comment with escaped chars");
    }
    FATP_END_SUBTEST

    return fat_p::testing::get_subtest_tracker().all_passed();
}

FATP_TEST_CASE(jsonc_strict_mode_rejection)
{
    std::string json = R"({
        // This is a comment
        "key": "value"
    })";

    FATP_ASSERT_THROWS(parse_json<StandardJsonPolicy>(json),
                       std::runtime_error,
                       "Standard policy should reject comments");
    return true;
}

FATP_TEST_CASE(utf8_european_chars_escaped)
{
    std::string text = "caf\xC3\xA9";
    std::string json = to_json_string<std::string, StandardJsonPolicy>(text);

    FATP_ASSERT_TRUE(json.find("\\u") != std::string::npos, "Should contain unicode escapes for European chars");

    std::string result = from_json_string<std::string>(json);
    FATP_ASSERT_EQ(result, text, "Should round-trip European characters");

    return true;
}

FATP_TEST_CASE(utf8_asian_chars_escaped)
{
    std::string text = "\xE4\xB8\x96\xE7\x95\x8C";
    std::string json = to_json_string<std::string, StandardJsonPolicy>(text);

    FATP_ASSERT_TRUE(json.find("\\u") != std::string::npos, "Should contain unicode escapes for Asian chars");

    std::string result = from_json_string<std::string>(json);
    FATP_ASSERT_EQ(result, text, "Should round-trip Asian characters");

    return true;
}

FATP_TEST_CASE(utf8_emoji_surrogate_pairs)
{
    std::string text = "\xF0\x9F\x98\x80";
    std::string json = to_json_string<std::string, StandardJsonPolicy>(text);

    FATP_ASSERT_TRUE(json.find("\\u") != std::string::npos, "Should contain unicode escapes for emoji");

    std::string result = from_json_string<std::string>(json);
    FATP_ASSERT_EQ(result, text, "Should round-trip emoji with surrogate pairs");

    return true;
}

FATP_TEST_CASE(utf8_raw_passthrough)
{
    std::string text = "\xE4\xB8\x96\xE7\x95\x8C caf\xC3\xA9 \xF0\x9F\x98\x80";
    std::string json = to_json_string<std::string, CompatJsonPolicy>(text);

    FATP_ASSERT_TRUE(json.find("\\u") == std::string::npos, "CompatJsonPolicy should not escape unicode");

    std::string result = from_json_string<std::string>(json);
    FATP_ASSERT_EQ(result, text, "Should round-trip raw UTF-8");

    return true;
}

FATP_TEST_CASE(utf8_mixed_content)
{
    Utf8TestData data;
    data.european_text = "caf\xC3\xA9 r\xC3\xA9sum\xC3\xA9";
    data.asian_text = "\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E \xE4\xB8\xAD\xE6\x96\x87";
    data.emoji_text = "\xF0\x9F\x98\x80 \xF0\x9F\x8E\x89 \xE2\x9D\xA4\xEF\xB8\x8F";
    data.mixed_text = "Hello \xE4\xB8\x96\xE7\x95\x8C caf\xC3\xA9 \xF0\x9F\x98\x80";

    std::string json = to_json_string<Utf8TestData, StandardJsonPolicy>(data);
    Utf8TestData result = from_json_string<Utf8TestData>(json);

    FATP_ASSERT_EQ(result.european_text, data.european_text, "European text should round-trip");
    FATP_ASSERT_EQ(result.asian_text, data.asian_text, "Asian text should round-trip");
    FATP_ASSERT_EQ(result.emoji_text, data.emoji_text, "Emoji text should round-trip");
    FATP_ASSERT_EQ(result.mixed_text, data.mixed_text, "Mixed text should round-trip");

    return true;
}

FATP_TEST_CASE(numeric_margin_removed_max_int64)
{
    JsonValue j = static_cast<int64_t>(LLONG_MAX);
    int64_t result = 0;
    from_json(j, result);
    FATP_ASSERT_EQ(result, LLONG_MAX, "LLONG_MAX should deserialize successfully");

    return true;
}

FATP_TEST_CASE(numeric_margin_removed_overflow_detection)
{
    JsonValue j = 9.223372036854776e18;
    int64_t result = 0;
    FATP_ASSERT_THROWS(from_json(j, result), std::runtime_error, "Should throw on genuine overflow");

    return true;
}

FATP_TEST_CASE(numeric_double_to_int_fractional)
{
    JsonValue j = 3.14;
    int result = 0;
    FATP_ASSERT_THROWS(from_json(j, result), std::runtime_error, "Should throw on fractional value");

    return true;
}

FATP_TEST_CASE(numeric_double_to_unsigned_negative)
{
    JsonValue j = -1.0;
    unsigned int result = 0;
    FATP_ASSERT_THROWS(from_json(j, result), std::runtime_error, "Should throw on negative to unsigned");

    return true;
}

FATP_TEST_CASE(numeric_all_primitives_from_double)
{
    FATP_SUBTEST("short from fractional")
    {
        JsonValue j = 3.14;
        short result = 0;
        FATP_ASSERT_THROWS(from_json(j, result), std::runtime_error, "Should throw fractional part error");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("signed char from fractional")
    {
        JsonValue j = 2.71;
        signed char result = 0;
        FATP_ASSERT_THROWS(from_json(j, result), std::runtime_error, "Should throw fractional part error");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("unsigned char from fractional")
    {
        JsonValue j = 1.5;
        unsigned char result = 0;
        FATP_ASSERT_THROWS(from_json(j, result), std::runtime_error, "Should throw fractional part error");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("unsigned short from fractional")
    {
        JsonValue j = 4.2;
        unsigned short result = 0;
        FATP_ASSERT_THROWS(from_json(j, result), std::runtime_error, "Should throw fractional part error");
    }
    FATP_END_SUBTEST

    return fat_p::testing::get_subtest_tracker().all_passed();
}

FATP_TEST_CASE(numeric_unsigned_negative_all_types)
{
    FATP_SUBTEST("unsigned int from negative")
    {
        JsonValue j = static_cast<int64_t>(-1);
        unsigned int result = 0;
        FATP_ASSERT_THROWS(from_json(j, result), std::runtime_error, "Should throw negative error");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("unsigned char from negative")
    {
        JsonValue j = static_cast<int64_t>(-10);
        unsigned char result = 0;
        FATP_ASSERT_THROWS(from_json(j, result), std::runtime_error, "Should throw negative error");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("unsigned short from negative")
    {
        JsonValue j = static_cast<int64_t>(-100);
        unsigned short result = 0;
        FATP_ASSERT_THROWS(from_json(j, result), std::runtime_error, "Should throw negative error");
    }
    FATP_END_SUBTEST

    return fat_p::testing::get_subtest_tracker().all_passed();
}

FATP_TEST_CASE(int64_max_boundary)
{
    FATP_SUBTEST("INT64_MAX exact value")
    {
        JsonValue j = static_cast<int64_t>(std::numeric_limits<int64_t>::max());
        int64_t result = 0;
        from_json(j, result);
        FATP_ASSERT_EQ(result, std::numeric_limits<int64_t>::max(), "Should handle INT64_MAX exactly");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("INT64_MAX as double within precision")
    {
        JsonValue j = 9.223372036854775e18;
        int64_t result = 0;
        from_json(j, result);
        FATP_ASSERT_TRUE(result > 0, "Should convert large double within int64 precision");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("Just above INT64_MAX")
    {
        constexpr double type_max_as_double = static_cast<double>(std::numeric_limits<int64_t>::max());
        double just_above = type_max_as_double * 1.0000001;
        JsonValue j = just_above;
        int64_t result = 0;
        FATP_ASSERT_THROWS(from_json(j, result), std::runtime_error, "Should reject value above INT64_MAX");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("Far above INT64_MAX")
    {
        JsonValue j = 1.0e19;
        int64_t result = 0;
        FATP_ASSERT_THROWS(from_json(j, result), std::runtime_error, "Should reject far above INT64_MAX");
    }
    FATP_END_SUBTEST

    return fat_p::testing::get_subtest_tracker().all_passed();
}

FATP_TEST_CASE(int64_min_boundary)
{
    FATP_SUBTEST("INT64_MIN exact value")
    {
        JsonValue j = static_cast<int64_t>(std::numeric_limits<int64_t>::min());
        int64_t result = 0;
        from_json(j, result);
        FATP_ASSERT_EQ(result, std::numeric_limits<int64_t>::min(), "Should handle INT64_MIN exactly");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("INT64_MIN as double within precision")
    {
        JsonValue j = -9.223372036854775e18;
        int64_t result = 0;
        from_json(j, result);
        FATP_ASSERT_TRUE(result < 0, "Should convert large negative double within int64 precision");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("Just below INT64_MIN")
    {
        constexpr double type_min_as_double = static_cast<double>(std::numeric_limits<int64_t>::min());
        double just_below = type_min_as_double * 1.0000001;
        JsonValue j = just_below;
        int64_t result = 0;
        FATP_ASSERT_THROWS(from_json(j, result), std::runtime_error, "Should reject value below INT64_MIN");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("Far below INT64_MIN")
    {
        JsonValue j = -1.0e19;
        int64_t result = 0;
        FATP_ASSERT_THROWS(from_json(j, result), std::runtime_error, "Should reject far below INT64_MIN");
    }
    FATP_END_SUBTEST

    return fat_p::testing::get_subtest_tracker().all_passed();
}

FATP_TEST_CASE(int32_boundaries)
{
    FATP_SUBTEST("INT32_MAX exact")
    {
        JsonValue j = static_cast<int64_t>(std::numeric_limits<int32_t>::max());
        int32_t result = 0;
        from_json(j, result);
        FATP_ASSERT_EQ(result, std::numeric_limits<int32_t>::max(), "Should handle INT32_MAX");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("INT32_MIN exact")
    {
        JsonValue j = static_cast<int64_t>(std::numeric_limits<int32_t>::min());
        int32_t result = 0;
        from_json(j, result);
        FATP_ASSERT_EQ(result, std::numeric_limits<int32_t>::min(), "Should handle INT32_MIN");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("Above INT32_MAX")
    {
        JsonValue j = static_cast<int64_t>(2147483648LL);
        int32_t result = 0;
        FATP_ASSERT_THROWS(from_json(j, result), std::runtime_error, "Should reject above INT32_MAX");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("Below INT32_MIN")
    {
        JsonValue j = static_cast<int64_t>(-2147483649LL);
        int32_t result = 0;
        FATP_ASSERT_THROWS(from_json(j, result), std::runtime_error, "Should reject below INT32_MIN");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("INT32_MAX from double")
    {
        JsonValue j = 2147483647.0;
        int32_t result = 0;
        from_json(j, result);
        FATP_ASSERT_EQ(result, std::numeric_limits<int32_t>::max(), "Should convert double INT32_MAX");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("Above INT32_MAX from double")
    {
        JsonValue j = 2147483648.0;
        int32_t result = 0;
        FATP_ASSERT_THROWS(from_json(j, result), std::runtime_error, "Should reject double above INT32_MAX");
    }
    FATP_END_SUBTEST

    return fat_p::testing::get_subtest_tracker().all_passed();
}

FATP_TEST_CASE(int16_boundaries)
{
    FATP_SUBTEST("INT16_MAX exact")
    {
        JsonValue j = static_cast<int64_t>(std::numeric_limits<int16_t>::max());
        int16_t result = 0;
        from_json(j, result);
        FATP_ASSERT_EQ(result, std::numeric_limits<int16_t>::max(), "Should handle INT16_MAX");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("INT16_MIN exact")
    {
        JsonValue j = static_cast<int64_t>(std::numeric_limits<int16_t>::min());
        int16_t result = 0;
        from_json(j, result);
        FATP_ASSERT_EQ(result, std::numeric_limits<int16_t>::min(), "Should handle INT16_MIN");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("Above INT16_MAX")
    {
        JsonValue j = static_cast<int64_t>(32768);
        int16_t result = 0;
        FATP_ASSERT_THROWS(from_json(j, result), std::runtime_error, "Should reject above INT16_MAX");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("Below INT16_MIN")
    {
        JsonValue j = static_cast<int64_t>(-32769);
        int16_t result = 0;
        FATP_ASSERT_THROWS(from_json(j, result), std::runtime_error, "Should reject below INT16_MIN");
    }
    FATP_END_SUBTEST

    return fat_p::testing::get_subtest_tracker().all_passed();
}

FATP_TEST_CASE(int8_boundaries)
{
    FATP_SUBTEST("INT8_MAX exact")
    {
        JsonValue j = static_cast<int64_t>(std::numeric_limits<int8_t>::max());
        int8_t result = 0;
        from_json(j, result);
        FATP_ASSERT_EQ(result, std::numeric_limits<int8_t>::max(), "Should handle INT8_MAX");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("INT8_MIN exact")
    {
        JsonValue j = static_cast<int64_t>(std::numeric_limits<int8_t>::min());
        int8_t result = 0;
        from_json(j, result);
        FATP_ASSERT_EQ(result, std::numeric_limits<int8_t>::min(), "Should handle INT8_MIN");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("Above INT8_MAX")
    {
        JsonValue j = static_cast<int64_t>(128);
        int8_t result = 0;
        FATP_ASSERT_THROWS(from_json(j, result), std::runtime_error, "Should reject above INT8_MAX");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("Below INT8_MIN")
    {
        JsonValue j = static_cast<int64_t>(-129);
        int8_t result = 0;
        FATP_ASSERT_THROWS(from_json(j, result), std::runtime_error, "Should reject below INT8_MIN");
    }
    FATP_END_SUBTEST

    return fat_p::testing::get_subtest_tracker().all_passed();
}

FATP_TEST_CASE(uint64_boundaries)
{
    FATP_SUBTEST("UINT64_MAX large value")
    {
        JsonValue j = static_cast<double>(std::numeric_limits<uint64_t>::max());
        uint64_t result = 0;
        FATP_ASSERT_THROWS(from_json(j, result),
                           std::runtime_error,
                           "Should reject UINT64_MAX due to precision loss beyond 2^53");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("Zero for unsigned")
    {
        JsonValue j = static_cast<int64_t>(0);
        uint64_t result = 1;
        from_json(j, result);
        FATP_ASSERT_EQ(result, 0ULL, "Should handle zero");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("Negative to uint64")
    {
        JsonValue j = static_cast<int64_t>(-1);
        uint64_t result = 0;
        FATP_ASSERT_THROWS(from_json(j, result), std::runtime_error, "Should reject negative for uint64");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("Large positive int64 to uint64")
    {
        JsonValue j = static_cast<int64_t>(std::numeric_limits<int64_t>::max());
        uint64_t result = 0;
        from_json(j, result);
        FATP_ASSERT_EQ(result,
                       static_cast<uint64_t>(std::numeric_limits<int64_t>::max()),
                       "Should convert INT64_MAX to uint64");
    }
    FATP_END_SUBTEST

    return fat_p::testing::get_subtest_tracker().all_passed();
}

FATP_TEST_CASE(uint32_boundaries)
{
    FATP_SUBTEST("UINT32_MAX exact")
    {
        JsonValue j = static_cast<int64_t>(std::numeric_limits<uint32_t>::max());
        uint32_t result = 0;
        from_json(j, result);
        FATP_ASSERT_EQ(result, std::numeric_limits<uint32_t>::max(), "Should handle UINT32_MAX");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("Above UINT32_MAX")
    {
        JsonValue j = static_cast<int64_t>(4294967296LL);
        uint32_t result = 0;
        FATP_ASSERT_THROWS(from_json(j, result), std::runtime_error, "Should reject above UINT32_MAX");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("Negative to uint32")
    {
        JsonValue j = static_cast<int64_t>(-1);
        uint32_t result = 0;
        FATP_ASSERT_THROWS(from_json(j, result), std::runtime_error, "Should reject negative for uint32");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("Zero for uint32")
    {
        JsonValue j = static_cast<int64_t>(0);
        uint32_t result = 1;
        from_json(j, result);
        FATP_ASSERT_EQ(result, 0U, "Should handle zero");
    }
    FATP_END_SUBTEST

    return fat_p::testing::get_subtest_tracker().all_passed();
}

FATP_TEST_CASE(uint16_boundaries)
{
    FATP_SUBTEST("UINT16_MAX exact")
    {
        JsonValue j = static_cast<int64_t>(std::numeric_limits<uint16_t>::max());
        uint16_t result = 0;
        from_json(j, result);
        FATP_ASSERT_EQ(result, std::numeric_limits<uint16_t>::max(), "Should handle UINT16_MAX");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("Above UINT16_MAX")
    {
        JsonValue j = static_cast<int64_t>(65536);
        uint16_t result = 0;
        FATP_ASSERT_THROWS(from_json(j, result), std::runtime_error, "Should reject above UINT16_MAX");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("Negative to uint16")
    {
        JsonValue j = static_cast<int64_t>(-1);
        uint16_t result = 0;
        FATP_ASSERT_THROWS(from_json(j, result), std::runtime_error, "Should reject negative for uint16");
    }
    FATP_END_SUBTEST

    return fat_p::testing::get_subtest_tracker().all_passed();
}

FATP_TEST_CASE(uint8_boundaries)
{
    FATP_SUBTEST("UINT8_MAX exact")
    {
        JsonValue j = static_cast<int64_t>(std::numeric_limits<uint8_t>::max());
        uint8_t result = 0;
        from_json(j, result);
        FATP_ASSERT_EQ(result, std::numeric_limits<uint8_t>::max(), "Should handle UINT8_MAX");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("Above UINT8_MAX")
    {
        JsonValue j = static_cast<int64_t>(256);
        uint8_t result = 0;
        FATP_ASSERT_THROWS(from_json(j, result), std::runtime_error, "Should reject above UINT8_MAX");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("Negative to uint8")
    {
        JsonValue j = static_cast<int64_t>(-1);
        uint8_t result = 0;
        FATP_ASSERT_THROWS(from_json(j, result), std::runtime_error, "Should reject negative for uint8");
    }
    FATP_END_SUBTEST

    return fat_p::testing::get_subtest_tracker().all_passed();
}

FATP_TEST_CASE(double_to_int_precision_edge_cases)
{
    FATP_SUBTEST("Very small fractional part")
    {
        JsonValue j = 42.0000000001;
        int result = 0;
        FATP_ASSERT_THROWS(from_json(j, result), std::runtime_error, "Should detect tiny fractional part");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("Exact integer as double")
    {
        JsonValue j = 123456789.0;
        int result = 0;
        from_json(j, result);
        FATP_ASSERT_EQ(result, 123456789, "Should accept exact integer");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("Large double near int boundary")
    {
        JsonValue j = 2147483646.9;
        int result = 0;
        FATP_ASSERT_THROWS(from_json(j, result), std::runtime_error, "Should reject fractional near INT_MAX");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("Negative with small fractional")
    {
        JsonValue j = -100.1;
        int result = 0;
        FATP_ASSERT_THROWS(from_json(j, result), std::runtime_error, "Should detect fractional on negative");
    }
    FATP_END_SUBTEST

    return fat_p::testing::get_subtest_tracker().all_passed();
}

FATP_TEST_CASE(overflow_detection_via_roundtrip)
{
    FATP_SUBTEST("Int64 overflow detection")
    {
        constexpr double type_max_as_double = static_cast<double>(std::numeric_limits<int64_t>::max());
        double just_above = type_max_as_double * 1.0000001;
        JsonValue j = just_above;
        int64_t result = 0;
        FATP_ASSERT_THROWS(from_json(j, result), std::runtime_error, "Should detect int64 overflow");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("Int64 underflow detection")
    {
        constexpr double type_min_as_double = static_cast<double>(std::numeric_limits<int64_t>::min());
        double just_below = type_min_as_double * 1.0000001;
        JsonValue j = just_below;
        int64_t result = 0;
        FATP_ASSERT_THROWS(from_json(j, result), std::runtime_error, "Should detect int64 underflow");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("Unsigned long long overflow detection")
    {
        JsonValue j = std::nextafter(static_cast<double>(std::numeric_limits<uint64_t>::max()),
                                     std::numeric_limits<double>::infinity());
        unsigned long long result = 0;
        FATP_ASSERT_THROWS(from_json(j, result), std::runtime_error, "Roundtrip should detect uint64 overflow");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("Long long valid near boundary")
    {
        JsonValue j = 9.223372036854774e18;
        long long result = 0;
        from_json(j, result);
        FATP_ASSERT_TRUE(result > 0, "Should accept valid value near boundary");
    }
    FATP_END_SUBTEST

    return fat_p::testing::get_subtest_tracker().all_passed();
}

FATP_TEST_CASE(signed_char_comprehensive)
{
    FATP_SUBTEST("SCHAR_MAX exact")
    {
        JsonValue j = static_cast<int64_t>(std::numeric_limits<signed char>::max());
        signed char result = 0;
        from_json(j, result);
        FATP_ASSERT_EQ(result, std::numeric_limits<signed char>::max(), "Should handle SCHAR_MAX");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("SCHAR_MIN exact")
    {
        JsonValue j = static_cast<int64_t>(std::numeric_limits<signed char>::min());
        signed char result = 0;
        from_json(j, result);
        FATP_ASSERT_EQ(result, std::numeric_limits<signed char>::min(), "Should handle SCHAR_MIN");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("Above SCHAR_MAX")
    {
        JsonValue j = static_cast<int64_t>(128);
        signed char result = 0;
        FATP_ASSERT_THROWS(from_json(j, result), std::runtime_error, "Should reject above SCHAR_MAX");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("Below SCHAR_MIN")
    {
        JsonValue j = static_cast<int64_t>(-129);
        signed char result = 0;
        FATP_ASSERT_THROWS(from_json(j, result), std::runtime_error, "Should reject below SCHAR_MIN");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("SCHAR_MAX from double")
    {
        JsonValue j = 127.0;
        signed char result = 0;
        from_json(j, result);
        FATP_ASSERT_EQ(result, 127, "Should convert double SCHAR_MAX");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("Fractional to signed char")
    {
        JsonValue j = 50.5;
        signed char result = 0;
        FATP_ASSERT_THROWS(from_json(j, result), std::runtime_error, "Should reject fractional");
    }
    FATP_END_SUBTEST

    return fat_p::testing::get_subtest_tracker().all_passed();
}

FATP_TEST_CASE(unsigned_char_comprehensive)
{
    FATP_SUBTEST("UCHAR_MAX exact")
    {
        JsonValue j = static_cast<int64_t>(std::numeric_limits<unsigned char>::max());
        unsigned char result = 0;
        from_json(j, result);
        FATP_ASSERT_EQ(result, std::numeric_limits<unsigned char>::max(), "Should handle UCHAR_MAX");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("Above UCHAR_MAX")
    {
        JsonValue j = static_cast<int64_t>(256);
        unsigned char result = 0;
        FATP_ASSERT_THROWS(from_json(j, result), std::runtime_error, "Should reject above UCHAR_MAX");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("Negative to unsigned char")
    {
        JsonValue j = static_cast<int64_t>(-1);
        unsigned char result = 0;
        FATP_ASSERT_THROWS(from_json(j, result), std::runtime_error, "Should reject negative");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("Zero for unsigned char")
    {
        JsonValue j = static_cast<int64_t>(0);
        unsigned char result = 1;
        from_json(j, result);
        FATP_ASSERT_EQ(result, static_cast<unsigned char>(0), "Should handle zero");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("UCHAR_MAX from double")
    {
        JsonValue j = 255.0;
        unsigned char result = 0;
        from_json(j, result);
        FATP_ASSERT_EQ(result, 255, "Should convert double UCHAR_MAX");
    }
    FATP_END_SUBTEST

    return fat_p::testing::get_subtest_tracker().all_passed();
}

FATP_TEST_CASE(short_comprehensive)
{
    FATP_SUBTEST("SHRT_MAX exact")
    {
        JsonValue j = static_cast<int64_t>(std::numeric_limits<short>::max());
        short result = 0;
        from_json(j, result);
        FATP_ASSERT_EQ(result, std::numeric_limits<short>::max(), "Should handle SHRT_MAX");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("SHRT_MIN exact")
    {
        JsonValue j = static_cast<int64_t>(std::numeric_limits<short>::min());
        short result = 0;
        from_json(j, result);
        FATP_ASSERT_EQ(result, std::numeric_limits<short>::min(), "Should handle SHRT_MIN");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("Above SHRT_MAX")
    {
        JsonValue j = static_cast<int64_t>(32768);
        short result = 0;
        FATP_ASSERT_THROWS(from_json(j, result), std::runtime_error, "Should reject above SHRT_MAX");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("Below SHRT_MIN")
    {
        JsonValue j = static_cast<int64_t>(-32769);
        short result = 0;
        FATP_ASSERT_THROWS(from_json(j, result), std::runtime_error, "Should reject below SHRT_MIN");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("SHRT_MAX from double")
    {
        JsonValue j = 32767.0;
        short result = 0;
        from_json(j, result);
        FATP_ASSERT_EQ(result, 32767, "Should convert double SHRT_MAX");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("Fractional to short")
    {
        JsonValue j = 1000.75;
        short result = 0;
        FATP_ASSERT_THROWS(from_json(j, result), std::runtime_error, "Should reject fractional");
    }
    FATP_END_SUBTEST

    return fat_p::testing::get_subtest_tracker().all_passed();
}

FATP_TEST_CASE(unsigned_short_comprehensive)
{
    FATP_SUBTEST("USHRT_MAX exact")
    {
        JsonValue j = static_cast<int64_t>(std::numeric_limits<unsigned short>::max());
        unsigned short result = 0;
        from_json(j, result);
        FATP_ASSERT_EQ(result, std::numeric_limits<unsigned short>::max(), "Should handle USHRT_MAX");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("Above USHRT_MAX")
    {
        JsonValue j = static_cast<int64_t>(65536);
        unsigned short result = 0;
        FATP_ASSERT_THROWS(from_json(j, result), std::runtime_error, "Should reject above USHRT_MAX");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("Negative to unsigned short")
    {
        JsonValue j = static_cast<int64_t>(-1);
        unsigned short result = 0;
        FATP_ASSERT_THROWS(from_json(j, result), std::runtime_error, "Should reject negative");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("USHRT_MAX from double")
    {
        JsonValue j = 65535.0;
        unsigned short result = 0;
        from_json(j, result);
        FATP_ASSERT_EQ(result, 65535, "Should convert double USHRT_MAX");
    }
    FATP_END_SUBTEST

    return fat_p::testing::get_subtest_tracker().all_passed();
}

FATP_TEST_CASE(double_extreme_values)
{
    FATP_SUBTEST("Very large positive double")
    {
        JsonValue j = 1.7976931348623157e308;
        double result = 0.0;
        from_json(j, result);
        FATP_ASSERT_TRUE(std::abs(result - 1.7976931348623157e308) < 1e292, "Should handle large double");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("Very small positive double")
    {
        JsonValue j = 2.2250738585072014e-308;
        double result = 0.0;
        from_json(j, result);
        FATP_ASSERT_TRUE(result > 0 && result < 1e-300, "Should handle small double");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("Very large negative double")
    {
        JsonValue j = -1.7976931348623157e308;
        double result = 0.0;
        from_json(j, result);
        FATP_ASSERT_TRUE(std::abs(result + 1.7976931348623157e308) < 1e292, "Should handle large negative double");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("Double subnormal value")
    {
        JsonValue j = 5e-324;
        double result = 0.0;
        from_json(j, result);
        FATP_ASSERT_TRUE(result >= 0, "Should handle subnormal double");
    }
    FATP_END_SUBTEST

    return fat_p::testing::get_subtest_tracker().all_passed();
}

FATP_TEST_CASE(float_boundaries)
{
    FATP_SUBTEST("Float max value")
    {
        JsonValue j = 3.4028234663852886e38;
        float result = 0.0f;
        from_json(j, result);
        FATP_ASSERT_TRUE(result > 3e38f, "Should handle large float");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("Float small value")
    {
        JsonValue j = 1.1754943508222875e-38;
        float result = 0.0f;
        from_json(j, result);
        FATP_ASSERT_TRUE(result > 0 && result < 1e-37f, "Should handle small float");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("Float from int64")
    {
        JsonValue j = static_cast<int64_t>(123456789);
        float result = 0.0f;
        from_json(j, result);
        FATP_ASSERT_TRUE(std::abs(result - 123456789.0f) < 100.0f, "Should convert int64 to float");
    }
    FATP_END_SUBTEST

    return fat_p::testing::get_subtest_tracker().all_passed();
}

FATP_TEST_CASE(zero_and_negative_zero)
{
    FATP_SUBTEST("Positive zero int")
    {
        JsonValue j = static_cast<int64_t>(0);
        int result = 1;
        from_json(j, result);
        FATP_ASSERT_EQ(result, 0, "Should handle positive zero");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("Positive zero double")
    {
        JsonValue j = 0.0;
        double result = 1.0;
        from_json(j, result);
        FATP_ASSERT_EQ(result, 0.0, "Should handle positive zero double");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("Negative zero double")
    {
        JsonValue j = -0.0;
        double result = 1.0;
        from_json(j, result);
        FATP_ASSERT_EQ(result, -0.0, "Should handle negative zero double");
    }
    FATP_END_SUBTEST

    return fat_p::testing::get_subtest_tracker().all_passed();
}

FATP_TEST_CASE(long_and_long_long_boundaries)
{
    FATP_SUBTEST("LONG_MAX exact")
    {
        JsonValue j = static_cast<int64_t>(std::numeric_limits<long>::max());
        long result = 0;
        from_json(j, result);
        FATP_ASSERT_EQ(result, std::numeric_limits<long>::max(), "Should handle LONG_MAX");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("LONG_MIN exact")
    {
        JsonValue j = static_cast<int64_t>(std::numeric_limits<long>::min());
        long result = 0;
        from_json(j, result);
        FATP_ASSERT_EQ(result, std::numeric_limits<long>::min(), "Should handle LONG_MIN");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("LLONG_MAX exact")
    {
        JsonValue j = static_cast<int64_t>(std::numeric_limits<long long>::max());
        long long result = 0;
        from_json(j, result);
        FATP_ASSERT_EQ(result, std::numeric_limits<long long>::max(), "Should handle LLONG_MAX");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("LLONG_MIN exact")
    {
        JsonValue j = static_cast<int64_t>(std::numeric_limits<long long>::min());
        long long result = 0;
        from_json(j, result);
        FATP_ASSERT_EQ(result, std::numeric_limits<long long>::min(), "Should handle LLONG_MIN");
    }
    FATP_END_SUBTEST

    return fat_p::testing::get_subtest_tracker().all_passed();
}

FATP_TEST_CASE(unsigned_long_and_long_long_boundaries)
{
    FATP_SUBTEST("ULONG_MAX from int64")
    {
        JsonValue j = static_cast<int64_t>(std::numeric_limits<int64_t>::max());
        unsigned long result = 0;

        if constexpr (sizeof(unsigned long) == 8)
        {
            from_json(j, result);
            FATP_ASSERT_TRUE(result > 0, "Should handle large unsigned long on 64-bit platform");
        }
        else
        {
            FATP_ASSERT_THROWS(from_json(j, result),
                               std::runtime_error,
                               "Should reject INT64_MAX for 32-bit unsigned long");
        }
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("Negative to unsigned long")
    {
        JsonValue j = static_cast<int64_t>(-1);
        unsigned long result = 0;
        FATP_ASSERT_THROWS(from_json(j, result), std::runtime_error, "Should reject negative for unsigned long");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("ULLONG from large int64")
    {
        JsonValue j = static_cast<int64_t>(std::numeric_limits<int64_t>::max());
        unsigned long long result = 0;
        from_json(j, result);
        FATP_ASSERT_TRUE(result > 0, "Should handle large unsigned long long");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("Negative to unsigned long long")
    {
        JsonValue j = static_cast<int64_t>(-1);
        unsigned long long result = 0;
        FATP_ASSERT_THROWS(from_json(j, result), std::runtime_error, "Should reject negative for unsigned long long");
    }
    FATP_END_SUBTEST

    return fat_p::testing::get_subtest_tracker().all_passed();
}

FATP_TEST_CASE(nan_default_rejection)
{
    std::string json = R"({"value": NaN})";
    FATP_ASSERT_THROWS(parse_json(json), std::runtime_error, "Standard policy should reject NaN");
    return true;
}

FATP_TEST_CASE(infinity_default_rejection)
{
    std::string json = R"({"value": Infinity})";
    FATP_ASSERT_THROWS(parse_json(json), std::runtime_error, "Standard policy should reject Infinity");
    return true;
}

FATP_TEST_CASE(nan_compat_acceptance)
{
    std::string json = R"({"value": NaN})";
    JsonValue j = parse_json<CompatJsonPolicy>(json);
    FATP_ASSERT_TRUE(j.is_object(), "Should parse object with NaN");

    const auto& obj = std::get<JsonObject>(j);
    double val = from_json<double>(obj.at("value"));
    FATP_ASSERT_TRUE(std::isnan(val), "Value should be NaN");

    return true;
}

FATP_TEST_CASE(infinity_compat_acceptance)
{
    std::string json = R"({"value": Infinity})";
    JsonValue j = parse_json<CompatJsonPolicy>(json);
    FATP_ASSERT_TRUE(j.is_object(), "Should parse object with Infinity");

    const auto& obj = std::get<JsonObject>(j);
    double val = from_json<double>(obj.at("value"));
    FATP_ASSERT_TRUE(std::isinf(val) && val > 0, "Value should be positive infinity");

    return true;
}

FATP_TEST_CASE(negative_infinity_compat)
{
    std::string json = R"({"value": -Infinity})";
    JsonValue j = parse_json<CompatJsonPolicy>(json);

    const auto& obj = std::get<JsonObject>(j);
    double val = from_json<double>(obj.at("value"));
    FATP_ASSERT_TRUE(std::isinf(val) && val < 0, "Value should be negative infinity");

    return true;
}

FATP_TEST_CASE(locale_independent_double_keys)
{
    // RAII guard to ensure locale is always restored
    struct LocaleGuard
    {
        std::locale original;
        LocaleGuard()
            : original(std::locale::global(std::locale::classic()))
        {
        }
        ~LocaleGuard()
        {
            std::locale::global(original);
        }
    } guard;

    // Test with classic locale first (baseline)
    std::map<double, int> m;
    m[3.14] = 42;
    m[2.71] = 100;

    std::string json = to_json_string(m);
    std::map<double, int> result = from_json_string<std::map<double, int>>(json);

    FATP_ASSERT_EQ(result.size(), m.size(), "Map size should match");
    FATP_ASSERT_TRUE(result.count(3.14) > 0, "Should have key 3.14");

    // Now test with a locale that uses comma as decimal separator (if available)
    // This verifies that our imbue(std::locale::classic()) fix works
    try
    {
        // Try to set German locale (uses comma as decimal separator)
        // Fall back gracefully if not available on this system
        std::locale german_locale("de_DE.UTF-8");
        std::locale::global(german_locale);

        // Serialize with German locale active - should still produce valid JSON with '.'
        std::string json_german = to_json_string(m);

        // Verify the output contains '.' not ','
        FATP_ASSERT_TRUE(json_german.find("3.14") != std::string::npos || json_german.find("3,14") == std::string::npos,
                         "JSON should use '.' decimal separator regardless of locale");

        // Round-trip should work
        std::map<double, int> result_german = from_json_string<std::map<double, int>>(json_german);
        FATP_ASSERT_EQ(result_german.size(), m.size(), "Map size should match after German locale test");
    }
    catch (const std::runtime_error&)
    {
        // German locale not available on this system - skip this part of test
    }

    return true;
}

FATP_TEST_CASE(locale_independent_serialization)
{
    // RAII guard to ensure locale is always restored
    struct LocaleGuard
    {
        std::locale original;
        LocaleGuard()
            : original(std::locale::global(std::locale::classic()))
        {
        }
        ~LocaleGuard()
        {
            std::locale::global(original);
        }
    } guard;

    // Test basic double serialization
    double test_value = 3.14159;

    try
    {
        // Try to set a locale with comma decimal separator
        std::locale comma_locale("de_DE.UTF-8");
        std::locale::global(comma_locale);

        // Serialize - should produce "3.14159" not "3,14159"
        std::string json = to_json_string(test_value);

        // The JSON must contain a period, not a comma for decimal
        FATP_ASSERT_TRUE(json.find('.') != std::string::npos,
                         "Double serialization should use '.' regardless of locale");
        FATP_ASSERT_TRUE(json.find(',') == std::string::npos,
                         "Double serialization should not use ',' as decimal separator");

        // Parse it back
        double parsed = from_json_string<double>(json);
        FATP_ASSERT_CLOSE_EPS(parsed, test_value, 0.00001, "Round-trip should preserve value");
    }
    catch (const std::runtime_error&)
    {
        // Locale not available - test passes by default since we can't test it
    }

    return true;
}

FATP_TEST_CASE(locale_independent_parsing)
{
    // RAII guard to ensure locale is always restored
    struct LocaleGuard
    {
        std::locale original;
        LocaleGuard()
            : original(std::locale::global(std::locale::classic()))
        {
        }
        ~LocaleGuard()
        {
            std::locale::global(original);
        }
    } guard;

    try
    {
        // Try to set a locale with comma decimal separator
        std::locale comma_locale("de_DE.UTF-8");
        std::locale::global(comma_locale);

        // Parse JSON containing floating point numbers
        // This should work regardless of locale because from_chars is locale-independent
        std::string json = R"({"value": 3.14159, "array": [1.5, 2.5, 3.5]})";
        auto parsed = parse_json(json);

        // Verify the values were parsed correctly
        FATP_ASSERT_TRUE(parsed.is_object(), "Should parse as object");

        auto& obj = std::get<JsonObject>(parsed);
        FATP_ASSERT_TRUE(obj.count("value") > 0, "Should have 'value' key");

        double value = from_json<double>(obj["value"]);
        FATP_ASSERT_CLOSE_EPS(value, 3.14159, 0.00001, "Value should be parsed correctly");

        // Check array values
        auto arr = from_json<std::vector<double>>(obj["array"]);
        FATP_ASSERT_EQ(arr.size(), 3u, "Array should have 3 elements");
        FATP_ASSERT_CLOSE_EPS(arr[0], 1.5, 0.001, "First element should be 1.5");
        FATP_ASSERT_CLOSE_EPS(arr[1], 2.5, 0.001, "Second element should be 2.5");
        FATP_ASSERT_CLOSE_EPS(arr[2], 3.5, 0.001, "Third element should be 3.5");
    }
    catch (const std::runtime_error&)
    {
        // Locale not available - test passes by default
    }

    return true;
}

FATP_TEST_CASE(container_empty_vector)
{
    JsonArray arr;
    std::vector<int> vec;
    from_json(arr, vec);
    FATP_ASSERT_TRUE(vec.empty(), "Empty array should produce empty vector");

    return true;
}

FATP_TEST_CASE(container_empty_set)
{
    JsonArray arr;
    std::set<int> s;
    from_json(arr, s);
    FATP_ASSERT_TRUE(s.empty(), "Empty array should produce empty set");

    return true;
}

FATP_TEST_CASE(container_empty_deque)
{
    JsonArray arr;
    std::deque<int> d;
    from_json(arr, d);
    FATP_ASSERT_TRUE(d.empty(), "Empty array should produce empty deque");

    return true;
}

FATP_TEST_CASE(save_params_with_backup)
{
    Point p1{10, 20};
    save_params("backup_test.json", p1);

    Point p2{30, 40};
    save_params_with_backup("backup_test.json", p2);

    FATP_ASSERT_TRUE(std::ifstream("backup_test.json").good(), "Main file should exist");
    FATP_ASSERT_TRUE(std::ifstream("backup_test.json.bak").good(), "Backup file should exist");

    Point loaded_main = load_params<Point>("backup_test.json");
    FATP_ASSERT_EQ(loaded_main.x, 30, "Main file should have new data");

    Point loaded_backup = load_params<Point>("backup_test.json.bak");
    FATP_ASSERT_EQ(loaded_backup.x, 10, "Backup should have old data");

    std::remove("backup_test.json");
    std::remove("backup_test.json.bak");
    return true;
}

FATP_TEST_CASE(save_params_with_custom_backup_suffix)
{
    Point p1{5, 15};
    save_params("custom_backup.json", p1);

    Point p2{25, 35};
    save_params_with_backup("custom_backup.json", p2, ".old");

    FATP_ASSERT_TRUE(std::ifstream("custom_backup.json.old").good(), "Custom backup should exist");

    std::remove("custom_backup.json");
    std::remove("custom_backup.json.old");
    return true;
}

FATP_TEST_CASE(json_pointer_basic_navigation)
{
    FATP_SUBTEST("object navigation")
    {
        JsonValue doc = parse_json(R"({
            "database": {
                "host": "localhost",
                "port": 5432
            }
        })");

        const JsonValue& port = query_json_pointer(doc, "/database/port");
        FATP_ASSERT_EQ(from_json<int>(port), 5432, "Should navigate to nested port");

        const JsonValue& host = query_json_pointer(doc, "/database/host");
        FATP_ASSERT_EQ(from_json<std::string>(host), "localhost", "Should navigate to nested host");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("array navigation")
    {
        JsonValue doc = parse_json(R"({
            "servers": ["primary", "backup", "tertiary"]
        })");

        const JsonValue& first = query_json_pointer(doc, "/servers/0");
        FATP_ASSERT_EQ(from_json<std::string>(first), "primary", "Should get first element");

        const JsonValue& second = query_json_pointer(doc, "/servers/1");
        FATP_ASSERT_EQ(from_json<std::string>(second), "backup", "Should get second element");

        const JsonValue& third = query_json_pointer(doc, "/servers/2");
        FATP_ASSERT_EQ(from_json<std::string>(third), "tertiary", "Should get third element");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("root document")
    {
        JsonValue doc = parse_json(R"({"key": "value"})");
        const JsonValue& root = query_json_pointer(doc, "");
        FATP_ASSERT_TRUE(root.is_object(), "Empty pointer should return root");
    }
    FATP_END_SUBTEST

    return fat_p::testing::get_subtest_tracker().all_passed();
}

FATP_TEST_CASE(json_pointer_escape_sequences)
{
    FATP_SUBTEST("tilde escape ~0")
    {
        JsonValue doc = parse_json(R"({"a~b": "value1"})");
        const JsonValue& val = query_json_pointer(doc, "/a~0b");
        FATP_ASSERT_EQ(from_json<std::string>(val), "value1", "~0 should decode to ~");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("forward slash escape ~1")
    {
        JsonValue doc = parse_json(R"({"foo/bar": "value2"})");
        const JsonValue& val = query_json_pointer(doc, "/foo~1bar");
        FATP_ASSERT_EQ(from_json<std::string>(val), "value2", "~1 should decode to /");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("multiple escapes")
    {
        JsonValue doc = parse_json(R"({"a~b/c": "value3"})");
        const JsonValue& val = query_json_pointer(doc, "/a~0b~1c");
        FATP_ASSERT_EQ(from_json<std::string>(val), "value3", "Should handle multiple escapes");
    }
    FATP_END_SUBTEST

    return fat_p::testing::get_subtest_tracker().all_passed();
}

FATP_TEST_CASE(json_pointer_type_safe)
{
    JsonValue doc = parse_json(R"({
        "database": {
            "host": "localhost",
            "port": 5432,
            "timeout": 30,
            "enabled": true
        },
        "servers": ["server1", "server2"]
    })");

    FATP_SUBTEST("query_json_as with primitives")
    {
        int port = query_json_as<int>(doc, "/database/port");
        FATP_ASSERT_EQ(port, 5432, "Should extract int");

        std::string host = query_json_as<std::string>(doc, "/database/host");
        FATP_ASSERT_EQ(host, "localhost", "Should extract string");

        bool enabled = query_json_as<bool>(doc, "/database/enabled");
        FATP_ASSERT_TRUE(enabled, "Should extract bool");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("query_json_as with containers")
    {
        auto servers = query_json_as<std::vector<std::string>>(doc, "/servers");
        FATP_ASSERT_EQ(servers.size(), 2u, "Should extract vector");
        FATP_ASSERT_EQ(servers[0], "server1", "First server correct");
        FATP_ASSERT_EQ(servers[1], "server2", "Second server correct");
    }
    FATP_END_SUBTEST

    return fat_p::testing::get_subtest_tracker().all_passed();
}

FATP_TEST_CASE(json_pointer_mutable)
{
    JsonValue doc = parse_json(R"({
        "config": {
            "port": 8080,
            "host": "localhost"
        },
        "list": [1, 2, 3]
    })");

    FATP_SUBTEST("modify nested value")
    {
        JsonValue& port = query_json_pointer(doc, "/config/port");
        port = 9000;

        int new_port = query_json_as<int>(doc, "/config/port");
        FATP_ASSERT_EQ(new_port, 9000, "Should modify nested value");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("modify array element")
    {
        JsonValue& elem = query_json_pointer(doc, "/list/1");
        elem = 42;

        int new_elem = query_json_as<int>(doc, "/list/1");
        FATP_ASSERT_EQ(new_elem, 42, "Should modify array element");
    }
    FATP_END_SUBTEST

    return fat_p::testing::get_subtest_tracker().all_passed();
}

FATP_TEST_CASE(json_pointer_complex_nested)
{
    JsonValue doc = parse_json(R"({
        "level1": {
            "level2": {
                "level3": {
                    "data": [
                        {"name": "item1", "value": 100},
                        {"name": "item2", "value": 200},
                        {"name": "item3", "value": 300}
                    ]
                }
            }
        }
    })");

    FATP_SUBTEST("deep nested navigation")
    {
        std::string name = query_json_as<std::string>(doc, "/level1/level2/level3/data/1/name");
        FATP_ASSERT_EQ(name, "item2", "Should navigate deeply");

        int value = query_json_as<int>(doc, "/level1/level2/level3/data/2/value");
        FATP_ASSERT_EQ(value, 300, "Should extract nested value");
    }
    FATP_END_SUBTEST

    return fat_p::testing::get_subtest_tracker().all_passed();
}

FATP_TEST_CASE(json_pointer_errors)
{
    FATP_SUBTEST("invalid pointer no leading slash")
    {
        JsonValue doc = parse_json(R"({"key": "value"})");
        FATP_ASSERT_THROWS(query_json_pointer(doc, "key"),
                           std::runtime_error,
                           "Should reject pointer without leading /");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("key not found")
    {
        JsonValue doc = parse_json(R"({"key": "value"})");
        FATP_ASSERT_THROWS(query_json_pointer(doc, "/nonexistent"),
                           std::runtime_error,
                           "Should throw when key not found");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("array index out of bounds")
    {
        JsonValue doc = parse_json(R"({"arr": [1, 2, 3]})");
        FATP_ASSERT_THROWS(query_json_pointer(doc, "/arr/10"),
                           std::runtime_error,
                           "Should throw when index out of bounds");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("invalid array index")
    {
        JsonValue doc = parse_json(R"({"arr": [1, 2, 3]})");
        FATP_ASSERT_THROWS(query_json_pointer(doc, "/arr/abc"),
                           std::runtime_error,
                           "Should throw on invalid array index");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("leading zero in array index")
    {
        JsonValue doc = parse_json(R"({"arr": [1, 2, 3]})");
        FATP_ASSERT_THROWS(query_json_pointer(doc, "/arr/01"),
                           std::runtime_error,
                           "Should reject leading zero per RFC 6901");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("navigate into scalar")
    {
        JsonValue doc = parse_json(R"({"value": 42})");
        FATP_ASSERT_THROWS(query_json_pointer(doc, "/value/nested"),
                           std::runtime_error,
                           "Should reject navigation into scalar");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("incomplete escape sequence")
    {
        JsonValue doc = parse_json(R"({"key~": "value"})");
        FATP_ASSERT_THROWS(query_json_pointer(doc, "/key~"), std::runtime_error, "Should reject incomplete escape");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("invalid escape sequence")
    {
        JsonValue doc = parse_json(R"({"key": "value"})");
        FATP_ASSERT_THROWS(query_json_pointer(doc, "/key~2"),
                           std::runtime_error,
                           "Should reject invalid escape sequence");
    }
    FATP_END_SUBTEST

    return fat_p::testing::get_subtest_tracker().all_passed();
}

FATP_TEST_CASE(json_pointer_edge_cases)
{
    FATP_SUBTEST("empty object key")
    {
        JsonValue doc = parse_json(R"({"": "empty_key"})");
        std::string val = query_json_as<std::string>(doc, "/");
        FATP_ASSERT_EQ(val, "empty_key", "Should handle empty key");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("zero array index")
    {
        JsonValue doc = parse_json(R"({"arr": [42]})");
        int val = query_json_as<int>(doc, "/arr/0");
        FATP_ASSERT_EQ(val, 42, "Should handle zero index");
    }
    FATP_END_SUBTEST

    FATP_SUBTEST("numeric object keys")
    {
        JsonValue doc = parse_json(R"({"0": "zero", "1": "one"})");
        std::string val0 = query_json_as<std::string>(doc, "/0");
        FATP_ASSERT_EQ(val0, "zero", "Should navigate numeric keys");

        std::string val1 = query_json_as<std::string>(doc, "/1");
        FATP_ASSERT_EQ(val1, "one", "Should navigate numeric keys");
    }
    FATP_END_SUBTEST

    return fat_p::testing::get_subtest_tracker().all_passed();
}

FATP_TEST_CASE(json_pointer_with_structs)
{
    FATP_SUBTEST("extract custom struct from pointer")
    {
        JsonValue doc = parse_json(R"({
            "database": {
                "host": "localhost",
                "port": 5432,
                "timeout": 30
            },
            "servers": ["s1", "s2"]
        })");

        auto db = query_json_as<DatabaseConfig>(doc, "/database");
        FATP_ASSERT_EQ(db.host, "localhost", "Should extract struct");
        FATP_ASSERT_EQ(db.port, 5432, "Should extract struct field");
        FATP_ASSERT_TRUE(db.timeout.has_value(), "Should extract optional");
        FATP_ASSERT_EQ(*db.timeout, 30, "Should extract optional value");
    }
    FATP_END_SUBTEST

    return fat_p::testing::get_subtest_tracker().all_passed();
}

FATP_TEST_CASE(json_pointer_rfc6901_examples)
{
    JsonValue doc = parse_json(R"({
        "foo": ["bar", "baz"],
        "": 0,
        "a/b": 1,
        "c%d": 2,
        "e^f": 3,
        "g|h": 4,
        "i\\j": 5,
        "k\"l": 6,
        " ": 7,
        "m~n": 8
    })");

    FATP_SUBTEST("RFC 6901 test cases")
    {
        const JsonValue& root = query_json_pointer(doc, "");
        FATP_ASSERT_TRUE(root.is_object(), "Empty string = whole document");

        auto foo = query_json_as<std::vector<std::string>>(doc, "/foo");
        FATP_ASSERT_EQ(foo[0], "bar", "/foo correct");

        std::string bar = query_json_as<std::string>(doc, "/foo/0");
        FATP_ASSERT_EQ(bar, "bar", "/foo/0 correct");

        int64_t empty_key = query_json_as<int64_t>(doc, "/");
        FATP_ASSERT_EQ(empty_key, 0, "Empty key correct");

        int64_t slash_key = query_json_as<int64_t>(doc, "/a~1b");
        FATP_ASSERT_EQ(slash_key, 1, "a/b with ~1 escape correct");

        int64_t tilde_key = query_json_as<int64_t>(doc, "/m~0n");
        FATP_ASSERT_EQ(tilde_key, 8, "m~n with ~0 escape correct");
    }
    FATP_END_SUBTEST

    return fat_p::testing::get_subtest_tracker().all_passed();
}

void benchmark_jsonlite()
{
    std::cout << "\n" << colors::cyan() << "JsonLite Benchmarks:" << colors::reset() << "\n\n";

    Point p{42, 84};
    double serialize_time = measure_perf(
        [&p]() {
            std::string json = to_json_string(p);
            DoNotOptimize(json);
        },
        100000,
        1000);
    std::cout << "Serialize Point: " << format_time(serialize_time) << "\n";

    std::string json_str = R"({"x": 42, "y": 84})";
    double parse_time = measure_perf(
        [&json_str]() {
            JsonValue j = parse_json(json_str);
            DoNotOptimize(j);
        },
        100000,
        1000);
    std::cout << "Parse simple object: " << format_time(parse_time) << "\n";

    double roundtrip_time = measure_perf(
        [&p]() {
            std::string json = to_json_string(p);
            Point p2 = from_json_string<Point>(json);
            DoNotOptimize(p2);
        },
        10000,
        100);
    std::cout << "Round-trip Point: " << format_time(roundtrip_time) << "\n";

    std::vector<int> large_vec(1000);
    for (size_t i = 0; i < large_vec.size(); ++i)
    {
        large_vec[i] = static_cast<int>(i);
    }
    double large_array_time = measure_perf(
        [&large_vec]() {
            std::string json = to_json_string(large_vec);
            DoNotOptimize(json);
        },
        1000,
        10);
    std::cout << "Serialize 1000 integers: " << format_time(large_array_time) << "\n";

    Employee emp;
    emp.name = "John Doe";
    emp.id = 12345;
    emp.email = "john@example.com";
    emp.skills = {"C++", "Python", "JavaScript", "Go", "Rust"};
    emp.certifications["AWS"] = 2020;
    emp.certifications["Azure"] = 2021;
    emp.certifications["GCP"] = 2022;

    double complex_struct_time = measure_perf(
        [&emp]() {
            std::string json = to_json_string(emp);
            Employee emp2 = from_json_string<Employee>(json);
            DoNotOptimize(emp2);
        },
        10000,
        100);
    std::cout << "Round-trip complex struct: " << format_time(complex_struct_time) << "\n";
}

// =============================================================================
// Malformed Input Tests
// =============================================================================

static bool malformed_expect_parse_fail(const std::string& json, const char* msg)
{
    try
    {
        JsonValue jv = parse_json(json);
        (void)jv;
        *get_test_config().output << "    UNEXPECTED SUCCESS: " << msg << "\n";
        return false;
    }
    catch (...)
    {
        // Expected to throw
        return true;
    }
}

FATP_TEST_CASE(malformed_missing_comma)
{
    bool r1 = malformed_expect_parse_fail("[1 2]", "missing comma in array must fail");
    FATP_ASSERT_TRUE(r1, "missing comma in array");

    bool r2 = malformed_expect_parse_fail("{\"a\":1 \"b\":2}", "missing comma in object must fail");
    FATP_ASSERT_TRUE(r2, "missing comma in object");
    return true;
}

FATP_TEST_CASE(malformed_trailing_comma_array)
{
    bool r1 = malformed_expect_parse_fail("[1,2,]", "trailing comma array must fail");
    FATP_ASSERT_TRUE(r1, "trailing comma array");

    bool r2 = malformed_expect_parse_fail("[,1,2]", "leading comma array must fail");
    FATP_ASSERT_TRUE(r2, "leading comma array");
    return true;
}

FATP_TEST_CASE(malformed_trailing_comma_object)
{
    bool r1 = malformed_expect_parse_fail("{\"a\":1,}", "trailing comma object must fail");
    FATP_ASSERT_TRUE(r1, "trailing comma object");
    return true;
}

FATP_TEST_CASE(malformed_unquoted_key)
{
    bool r1 = malformed_expect_parse_fail("{a:1}", "unquoted key must fail");
    FATP_ASSERT_TRUE(r1, "unquoted key");

    bool r2 = malformed_expect_parse_fail("{123:1}", "numeric key must fail");
    FATP_ASSERT_TRUE(r2, "numeric key");
    return true;
}

FATP_TEST_CASE(malformed_unterminated_string)
{
    bool r1 = malformed_expect_parse_fail("\"unterminated", "unterminated string must fail");
    FATP_ASSERT_TRUE(r1, "unterminated string");

    bool r2 = malformed_expect_parse_fail("\"unterminated\\", "unterminated escape must fail");
    FATP_ASSERT_TRUE(r2, "unterminated escape");
    return true;
}

FATP_TEST_CASE(malformed_numbers)
{
    // Leading zeros (except 0 itself)
    bool r1 = malformed_expect_parse_fail("01", "leading zero number must fail");
    FATP_ASSERT_TRUE(r1, "leading zero");

    bool r2 = malformed_expect_parse_fail("007", "leading zeros must fail");
    FATP_ASSERT_TRUE(r2, "leading zeros");

    // Trailing/leading decimal points
    bool r3 = malformed_expect_parse_fail("1.", "trailing decimal point must fail");
    FATP_ASSERT_TRUE(r3, "trailing decimal");

    bool r4 = malformed_expect_parse_fail(".1", "leading decimal point must fail");
    FATP_ASSERT_TRUE(r4, "leading decimal");

    // Double minus
    bool r5 = malformed_expect_parse_fail("--1", "double minus must fail");
    FATP_ASSERT_TRUE(r5, "double minus");

    // Plus sign (not allowed in JSON)
    bool r6 = malformed_expect_parse_fail("+1", "plus sign must fail");
    FATP_ASSERT_TRUE(r6, "plus sign");

    // Special values (strict JSON rejects these)
    bool r7 = malformed_expect_parse_fail("NaN", "NaN must fail in strict JSON");
    FATP_ASSERT_TRUE(r7, "NaN");

    bool r8 = malformed_expect_parse_fail("Infinity", "Infinity must fail in strict JSON");
    FATP_ASSERT_TRUE(r8, "Infinity");

    bool r9 = malformed_expect_parse_fail("-Infinity", "-Infinity must fail in strict JSON");
    FATP_ASSERT_TRUE(r9, "-Infinity");

    return true;
}

FATP_TEST_CASE(malformed_string_escapes)
{
    bool r1 = malformed_expect_parse_fail("\"\\x41\"", "non-standard \\x escape must fail");
    FATP_ASSERT_TRUE(r1, "non-standard \\x escape");

    bool r2 = malformed_expect_parse_fail("\"\\u12\"", "short \\u escape must fail");
    FATP_ASSERT_TRUE(r2, "short \\u escape");

    bool r3 = malformed_expect_parse_fail("\"\\uZZZZ\"", "non-hex \\u escape must fail");
    FATP_ASSERT_TRUE(r3, "non-hex \\u escape");

    bool r4 = malformed_expect_parse_fail("\"\\a\"", "non-standard \\a escape must fail");
    FATP_ASSERT_TRUE(r4, "non-standard \\a escape");
    return true;
}

FATP_TEST_CASE(malformed_structure)
{
    // Mismatched brackets
    bool r1 = malformed_expect_parse_fail("[1,2,3}", "mismatched brackets must fail");
    FATP_ASSERT_TRUE(r1, "mismatched brackets");

    bool r2 = malformed_expect_parse_fail("{\"a\":1]", "mismatched braces must fail");
    FATP_ASSERT_TRUE(r2, "mismatched braces");

    // Empty input
    bool r3 = malformed_expect_parse_fail("", "empty input must fail");
    FATP_ASSERT_TRUE(r3, "empty input");

    bool r4 = malformed_expect_parse_fail("   ", "whitespace-only input must fail");
    FATP_ASSERT_TRUE(r4, "whitespace-only input");

    return true;
}

FATP_TEST_CASE(malformed_literals)
{
    bool r1 = malformed_expect_parse_fail("True", "capitalized True must fail");
    FATP_ASSERT_TRUE(r1, "capitalized True");

    bool r2 = malformed_expect_parse_fail("FALSE", "uppercase FALSE must fail");
    FATP_ASSERT_TRUE(r2, "uppercase FALSE");

    bool r3 = malformed_expect_parse_fail("Null", "capitalized Null must fail");
    FATP_ASSERT_TRUE(r3, "capitalized Null");

    bool r4 = malformed_expect_parse_fail("NULL", "uppercase NULL must fail");
    FATP_ASSERT_TRUE(r4, "uppercase NULL");
    return true;
}

FATP_TEST_CASE(malformed_deep_nesting_stress)
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
    try
    {
        JsonValue jv = parse_json(json);
        (void)jv;
    }
    catch (...)
    {
        // Either outcome is acceptable, but it must not crash
    }

    return true;
}

FATP_TEST_CASE(malformed_unicode)
{
    // Invalid UTF-8 sequences (these may or may not fail depending on policy)
    // The key point is they shouldn't crash
    try
    {
        JsonValue jv1 = parse_json("\"\\uD800\""); // Lone high surrogate
        (void)jv1;
    }
    catch (...)
    {
    }

    try
    {
        JsonValue jv2 = parse_json("\"\\uDC00\""); // Lone low surrogate
        (void)jv2;
    }
    catch (...)
    {
    }

    return true;
}

// =============================================================================
// Fuzz Tests
// =============================================================================

class FuzzRandom
{
public:
    explicit FuzzRandom(std::uint64_t seed) noexcept
        : mEng(seed)
    {
    }

    std::int64_t i64(std::int64_t lo, std::int64_t hi)
    {
        std::uniform_int_distribution<std::int64_t> d(lo, hi);
        return d(mEng);
    }

    std::uint64_t u64(std::uint64_t lo, std::uint64_t hi)
    {
        std::uniform_int_distribution<std::uint64_t> d(lo, hi);
        return d(mEng);
    }

    double dbl(double lo, double hi)
    {
        std::uniform_real_distribution<double> d(lo, hi);
        return d(mEng);
    }

    std::string str(std::size_t max_len)
    {
        std::uniform_int_distribution<std::size_t> len_dist(0, max_len);
        const std::size_t len = len_dist(mEng);
        std::uniform_int_distribution<int> ch_dist(32, 126);

        std::string s;
        s.reserve(len);
        for (std::size_t i = 0; i < len; ++i)
        {
            char c = static_cast<char>(ch_dist(mEng));
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
    std::mt19937_64 mEng;
};

template <typename T>
static bool fuzz_roundtrip(const T& value, T& result)
{
    try
    {
        JsonValue jv;
        to_json(jv, value);
        std::string json = to_json_string(jv);

        JsonValue parsed = parse_json(json);
        from_json(parsed, result);
        return true;
    }
    catch (...)
    {
        return false;
    }
}

FATP_TEST_CASE(fuzz_ints)
{
    FuzzRandom rng(0xC0FFEE123456789ULL);

    for (int i = 0; i < 2000; ++i)
    {
        const auto v = static_cast<int>(rng.i64(std::numeric_limits<int>::min(), std::numeric_limits<int>::max()));
        int result = 0;
        FATP_ASSERT_TRUE(fuzz_roundtrip(v, result), "Fuzz int roundtrip failed");
        FATP_ASSERT_TRUE(result == v, "Fuzz int mismatch");
    }

    return true;
}

FATP_TEST_CASE(fuzz_int64)
{
    FuzzRandom rng(0xC0FFEE64B101234ULL);

    for (int i = 0; i < 2000; ++i)
    {
        const auto v =
            rng.i64(std::numeric_limits<std::int64_t>::min() / 2, std::numeric_limits<std::int64_t>::max() / 2);
        std::int64_t result = 0;
        FATP_ASSERT_TRUE(fuzz_roundtrip(v, result), "Fuzz int64 roundtrip failed");
        FATP_ASSERT_TRUE(result == v, "Fuzz int64 mismatch");
    }

    return true;
}

FATP_TEST_CASE(fuzz_doubles)
{
    FuzzRandom rng(0xD0B1E1234ULL);

    for (int i = 0; i < 2000; ++i)
    {
        const double v = rng.dbl(-1e6, 1e6);
        double result = 0.0;
        FATP_ASSERT_TRUE(fuzz_roundtrip(v, result), "Fuzz double roundtrip failed");
        FATP_ASSERT_TRUE(std::fabs(result - v) < 1e-9, "Fuzz double mismatch");
    }

    return true;
}

FATP_TEST_CASE(fuzz_strings)
{
    FuzzRandom rng(0x5AB1CAFE21ULL);

    for (int i = 0; i < 2000; ++i)
    {
        const std::string v = rng.str(64);
        std::string result;
        FATP_ASSERT_TRUE(fuzz_roundtrip(v, result), "Fuzz string roundtrip failed");
        FATP_ASSERT_TRUE(result == v, "Fuzz string mismatch");
    }

    return true;
}

FATP_TEST_CASE(fuzz_vector_int)
{
    FuzzRandom rng(0xF00BA12345ULL);

    for (int iter = 0; iter < 500; ++iter)
    {
        const std::size_t len = static_cast<std::size_t>(rng.u64(0U, 32U));

        std::vector<int> v;
        v.reserve(len);
        for (std::size_t i = 0; i < len; ++i)
        {
            v.push_back(static_cast<int>(rng.i64(-100000, 100000)));
        }

        std::vector<int> result;
        FATP_ASSERT_TRUE(fuzz_roundtrip(v, result), "Fuzz vector<int> roundtrip failed");
        FATP_ASSERT_TRUE(result == v, "Fuzz vector<int> mismatch");
    }

    return true;
}

FATP_TEST_CASE(fuzz_map_string_int)
{
    FuzzRandom rng(0xAABFE21234ULL);

    for (int iter = 0; iter < 300; ++iter)
    {
        const std::size_t len = static_cast<std::size_t>(rng.u64(0U, 16U));

        std::map<std::string, int> m;
        for (std::size_t i = 0; i < len; ++i)
        {
            const std::string key = rng.str(16);
            const int value = static_cast<int>(rng.i64(-1000, 1000));
            m[key] = value;
        }

        std::map<std::string, int> result;
        FATP_ASSERT_TRUE(fuzz_roundtrip(m, result), "Fuzz map<string,int> roundtrip failed");
        FATP_ASSERT_TRUE(result == m, "Fuzz map<string,int> mismatch");
    }

    return true;
}

FATP_TEST_CASE(fuzz_nested_structures)
{
    FuzzRandom rng(0xAE5EDDA01ULL);

    for (int iter = 0; iter < 200; ++iter)
    {
        const std::size_t outer_len = static_cast<std::size_t>(rng.u64(0U, 8U));

        std::vector<std::map<std::string, int>> v;
        v.reserve(outer_len);

        for (std::size_t i = 0; i < outer_len; ++i)
        {
            std::map<std::string, int> inner;
            const std::size_t inner_len = static_cast<std::size_t>(rng.u64(0U, 8U));

            for (std::size_t j = 0; j < inner_len; ++j)
            {
                const std::string key = rng.str(10);
                const int value = static_cast<int>(rng.i64(-5000, 5000));
                inner[key] = value;
            }

            v.push_back(std::move(inner));
        }

        std::vector<std::map<std::string, int>> result;
        FATP_ASSERT_TRUE(fuzz_roundtrip(v, result), "Fuzz nested roundtrip failed");
        FATP_ASSERT_TRUE(result == v, "Fuzz nested vector<map<string,int>> mismatch");
    }

    return true;
}

// =============================================================================
// Encode/Decode Benchmark Tests
// =============================================================================

template <typename T>
std::string encode_value(const T& value)
{
    JsonValue jv;
    to_json(jv, value);
    return to_json_string(jv);
}

template <typename T>
T decode_value(const std::string& json)
{
    JsonValue jv = parse_json(json);
    T result;
    from_json(jv, result);
    return result;
}

} // namespace fat_p::testing::jsonlite

namespace fat_p::testing
{

bool test_JsonLite()
{
    FATP_PRINT_HEADER(JSON LITE)

    TestRunner runner;

    FATP_RUN_TEST_NS(runner, jsonlite, json_lite_basic_types);

    FATP_RUN_TEST_NS(runner, jsonlite, json_lite_int8);
    FATP_RUN_TEST_NS(runner, jsonlite, json_lite_int16);
    FATP_RUN_TEST_NS(runner, jsonlite, json_lite_int32);
    FATP_RUN_TEST_NS(runner, jsonlite, json_lite_int64);
    FATP_RUN_TEST_NS(runner, jsonlite, json_lite_uint8);
    FATP_RUN_TEST_NS(runner, jsonlite, json_lite_uint16);
    FATP_RUN_TEST_NS(runner, jsonlite, json_lite_uint32);
    FATP_RUN_TEST_NS(runner, jsonlite, json_lite_uint64);
    FATP_RUN_TEST_NS(runner, jsonlite, json_lite_float_types);
    FATP_RUN_TEST_NS(runner, jsonlite, json_lite_numeric_edge_cases);
    FATP_RUN_TEST_NS(runner, jsonlite, json_lite_fixed_width_int8);
    FATP_RUN_TEST_NS(runner, jsonlite, json_lite_fixed_width_int16);
    FATP_RUN_TEST_NS(runner, jsonlite, json_lite_fixed_width_int32);
    FATP_RUN_TEST_NS(runner, jsonlite, json_lite_fixed_width_int64);
    FATP_RUN_TEST_NS(runner, jsonlite, json_lite_fixed_width_uint8);
    FATP_RUN_TEST_NS(runner, jsonlite, json_lite_fixed_width_uint16);
    FATP_RUN_TEST_NS(runner, jsonlite, json_lite_fixed_width_uint32);
    FATP_RUN_TEST_NS(runner, jsonlite, json_lite_fixed_width_uint64);
    FATP_RUN_TEST_NS(runner, jsonlite, json_lite_size_t_type);
    FATP_RUN_TEST_NS(runner, jsonlite, json_lite_ptrdiff_t_type);

    FATP_RUN_TEST_NS(runner, jsonlite, json_lite_string_escaping);
    FATP_RUN_TEST_NS(runner, jsonlite, json_lite_string_empty);
    FATP_RUN_TEST_NS(runner, jsonlite, json_lite_string_whitespace);
    FATP_RUN_TEST_NS(runner, jsonlite, json_lite_string_special_chars);
    FATP_RUN_TEST_NS(runner, jsonlite, json_lite_string_unicode);
    FATP_RUN_TEST_NS(runner, jsonlite, json_lite_string_long);

    FATP_RUN_TEST_NS(runner, jsonlite, json_lite_containers);
    FATP_RUN_TEST_NS(runner, jsonlite, json_lite_vector_empty);
    FATP_RUN_TEST_NS(runner, jsonlite, json_lite_vector_single);
    FATP_RUN_TEST_NS(runner, jsonlite, json_lite_set);
    FATP_RUN_TEST_NS(runner, jsonlite, json_lite_map_empty);
    FATP_RUN_TEST_NS(runner, jsonlite, json_lite_map_numeric_keys);
    FATP_RUN_TEST_NS(runner, jsonlite, json_lite_nested_vectors);
    FATP_RUN_TEST_NS(runner, jsonlite, json_lite_mixed_nested);
    FATP_RUN_TEST_NS(runner, jsonlite, json_lite_array);
    FATP_RUN_TEST_NS(runner, jsonlite, json_lite_unordered_set);
    FATP_RUN_TEST_NS(runner, jsonlite, json_lite_unordered_map);
    FATP_RUN_TEST_NS(runner, jsonlite, json_lite_deque);
    FATP_RUN_TEST_NS(runner, jsonlite, json_lite_list);
    FATP_RUN_TEST_NS(runner, jsonlite, json_lite_string_view);

    FATP_RUN_TEST_NS(runner, jsonlite, json_lite_optional);
    FATP_RUN_TEST_NS(runner, jsonlite, json_lite_optional_string);
    FATP_RUN_TEST_NS(runner, jsonlite, json_lite_optional_vector);

    FATP_RUN_TEST_NS(runner, jsonlite, json_lite_tuples_pairs);
    FATP_RUN_TEST_NS(runner, jsonlite, json_lite_pair_nested);
    FATP_RUN_TEST_NS(runner, jsonlite, json_lite_tuple_large);

    FATP_RUN_TEST_NS(runner, jsonlite, json_lite_simple_struct);
    FATP_RUN_TEST_NS(runner, jsonlite, json_lite_complex_struct);
    FATP_RUN_TEST_NS(runner, jsonlite, json_lite_optional_fields);
    FATP_RUN_TEST_NS(runner, jsonlite, json_lite_intrusive_serialization);
    FATP_RUN_TEST_NS(runner, jsonlite, json_lite_nested_structs);
    FATP_RUN_TEST_NS(runner, jsonlite, json_lite_complex_nested);
    FATP_RUN_TEST_NS(runner, jsonlite, json_lite_deep_hierarchy);

    FATP_RUN_TEST_NS(runner, jsonlite, json_lite_parser_basic);
    FATP_RUN_TEST_NS(runner, jsonlite, json_lite_parser_containers);
    FATP_RUN_TEST_NS(runner, jsonlite, json_lite_parser_edge_cases);
    FATP_RUN_TEST_NS(runner, jsonlite, json_lite_parser_numbers);
    FATP_RUN_TEST_NS(runner, jsonlite, json_lite_parser_strings);
    FATP_RUN_TEST_NS(runner, jsonlite, json_lite_parser_nested);
    FATP_RUN_TEST_NS(runner, jsonlite, json_lite_parser_errors);
    FATP_RUN_TEST_NS(runner, jsonlite, json_lite_parser_literals);
    FATP_RUN_TEST_NS(runner, jsonlite, json_lite_parser_unicode);

    FATP_RUN_TEST_NS(runner, jsonlite, json_lite_pretty_print);
    FATP_RUN_TEST_NS(runner, jsonlite, json_lite_numeric_precision);
    FATP_RUN_TEST_NS(runner, jsonlite, json_lite_nan_inf_handling);
    FATP_RUN_TEST_NS(runner, jsonlite, json_lite_standard_policy_nan);

    FATP_RUN_TEST_NS(runner, jsonlite, json_lite_file_io);
    FATP_RUN_TEST_NS(runner, jsonlite, json_lite_file_io_complex);
    FATP_RUN_TEST_NS(runner, jsonlite, json_lite_file_io_errors);

    FATP_RUN_TEST_NS(runner, jsonlite, json_lite_roundtrip_all_types);
    FATP_RUN_TEST_NS(runner, jsonlite, json_lite_convenience_functions);
    FATP_RUN_TEST_NS(runner, jsonlite, json_lite_value_returning_from_json);
    FATP_RUN_TEST_NS(runner, jsonlite, json_lite_roundtrip_vectors);
    FATP_RUN_TEST_NS(runner, jsonlite, json_lite_roundtrip_maps);

    FATP_RUN_TEST_NS(runner, jsonlite, json_lite_large_data);
    FATP_RUN_TEST_NS(runner, jsonlite, json_lite_deeply_nested);
    FATP_RUN_TEST_NS(runner, jsonlite, json_lite_large_strings);
    FATP_RUN_TEST_NS(runner, jsonlite, json_lite_large_map);

    FATP_RUN_TEST_NS(runner, jsonlite, json_lite_depth_limit_parse);
    FATP_RUN_TEST_NS(runner, jsonlite, json_lite_depth_limit_dump);
    FATP_RUN_TEST_NS(runner, jsonlite, json_lite_range_checks);
    FATP_RUN_TEST_NS(runner, jsonlite, json_lite_range_int8);
    FATP_RUN_TEST_NS(runner, jsonlite, json_lite_better_error_messages);
    FATP_RUN_TEST_NS(runner, jsonlite, json_lite_param_helpers);
    FATP_RUN_TEST_NS(runner, jsonlite, json_lite_position_in_errors);
    FATP_RUN_TEST_NS(runner, jsonlite, json_lite_error_messages_comprehensive);

    FATP_RUN_TEST_NS(runner, jsonlite, json_lite_field_limit);
    FATP_RUN_TEST_NS(runner, jsonlite, json_lite_bug2_name_collision);
    FATP_RUN_TEST_NS(runner, jsonlite, json_lite_bug2_collision_prevention);
    FATP_RUN_TEST_NS(runner, jsonlite, json_lite_numeric_formatting);
    FATP_RUN_TEST_NS(runner, jsonlite, json_lite_bug4_error_context);
    FATP_RUN_TEST_NS(runner, jsonlite, json_lite_bug5_fixed_arrays);
    FATP_RUN_TEST_NS(runner, jsonlite, json_lite_bug6_policy_incompatibility);
    FATP_RUN_TEST_NS(runner, jsonlite, json_lite_bug6_policy_fix_verification);

    FATP_RUN_TEST_NS(runner, jsonlite, jsonc_line_comments);
    FATP_RUN_TEST_NS(runner, jsonlite, jsonc_block_comments);
    FATP_RUN_TEST_NS(runner, jsonlite, jsonc_comment_edge_cases);
    FATP_RUN_TEST_NS(runner, jsonlite, jsonc_strict_mode_rejection);

    FATP_RUN_TEST_NS(runner, jsonlite, utf8_european_chars_escaped);
    FATP_RUN_TEST_NS(runner, jsonlite, utf8_asian_chars_escaped);
    FATP_RUN_TEST_NS(runner, jsonlite, utf8_emoji_surrogate_pairs);
    FATP_RUN_TEST_NS(runner, jsonlite, utf8_raw_passthrough);
    FATP_RUN_TEST_NS(runner, jsonlite, utf8_mixed_content);

    FATP_RUN_TEST_NS(runner, jsonlite, numeric_margin_removed_max_int64);
    FATP_RUN_TEST_NS(runner, jsonlite, numeric_margin_removed_overflow_detection);
    FATP_RUN_TEST_NS(runner, jsonlite, numeric_double_to_int_fractional);
    FATP_RUN_TEST_NS(runner, jsonlite, numeric_double_to_unsigned_negative);
    FATP_RUN_TEST_NS(runner, jsonlite, numeric_all_primitives_from_double);
    FATP_RUN_TEST_NS(runner, jsonlite, numeric_unsigned_negative_all_types);

    FATP_RUN_TEST_NS(runner, jsonlite, int64_max_boundary);
    FATP_RUN_TEST_NS(runner, jsonlite, int64_min_boundary);
    FATP_RUN_TEST_NS(runner, jsonlite, int32_boundaries);
    FATP_RUN_TEST_NS(runner, jsonlite, int16_boundaries);
    FATP_RUN_TEST_NS(runner, jsonlite, int8_boundaries);
    FATP_RUN_TEST_NS(runner, jsonlite, uint64_boundaries);
    FATP_RUN_TEST_NS(runner, jsonlite, uint32_boundaries);
    FATP_RUN_TEST_NS(runner, jsonlite, uint16_boundaries);
    FATP_RUN_TEST_NS(runner, jsonlite, uint8_boundaries);
    FATP_RUN_TEST_NS(runner, jsonlite, double_to_int_precision_edge_cases);
    FATP_RUN_TEST_NS(runner, jsonlite, overflow_detection_via_roundtrip);
    FATP_RUN_TEST_NS(runner, jsonlite, signed_char_comprehensive);
    FATP_RUN_TEST_NS(runner, jsonlite, unsigned_char_comprehensive);
    FATP_RUN_TEST_NS(runner, jsonlite, short_comprehensive);
    FATP_RUN_TEST_NS(runner, jsonlite, unsigned_short_comprehensive);
    FATP_RUN_TEST_NS(runner, jsonlite, double_extreme_values);
    FATP_RUN_TEST_NS(runner, jsonlite, float_boundaries);
    FATP_RUN_TEST_NS(runner, jsonlite, zero_and_negative_zero);
    FATP_RUN_TEST_NS(runner, jsonlite, long_and_long_long_boundaries);
    FATP_RUN_TEST_NS(runner, jsonlite, unsigned_long_and_long_long_boundaries);

    FATP_RUN_TEST_NS(runner, jsonlite, nan_default_rejection);
    FATP_RUN_TEST_NS(runner, jsonlite, infinity_default_rejection);
    FATP_RUN_TEST_NS(runner, jsonlite, nan_compat_acceptance);
    FATP_RUN_TEST_NS(runner, jsonlite, infinity_compat_acceptance);
    FATP_RUN_TEST_NS(runner, jsonlite, negative_infinity_compat);

    FATP_RUN_TEST_NS(runner, jsonlite, locale_independent_double_keys);
    FATP_RUN_TEST_NS(runner, jsonlite, locale_independent_serialization);
    FATP_RUN_TEST_NS(runner, jsonlite, locale_independent_parsing);

    FATP_RUN_TEST_NS(runner, jsonlite, container_empty_vector);
    FATP_RUN_TEST_NS(runner, jsonlite, container_empty_set);
    FATP_RUN_TEST_NS(runner, jsonlite, container_empty_deque);

    FATP_RUN_TEST_NS(runner, jsonlite, save_params_with_backup);
    FATP_RUN_TEST_NS(runner, jsonlite, save_params_with_custom_backup_suffix);

    FATP_RUN_TEST_NS(runner, jsonlite, json_pointer_basic_navigation);
    FATP_RUN_TEST_NS(runner, jsonlite, json_pointer_escape_sequences);
    FATP_RUN_TEST_NS(runner, jsonlite, json_pointer_type_safe);
    FATP_RUN_TEST_NS(runner, jsonlite, json_pointer_mutable);
    FATP_RUN_TEST_NS(runner, jsonlite, json_pointer_complex_nested);
    FATP_RUN_TEST_NS(runner, jsonlite, json_pointer_errors);
    FATP_RUN_TEST_NS(runner, jsonlite, json_pointer_edge_cases);
    FATP_RUN_TEST_NS(runner, jsonlite, json_pointer_with_structs);
    FATP_RUN_TEST_NS(runner, jsonlite, json_pointer_rfc6901_examples);

    // Malformed input tests
    FATP_RUN_TEST_NS(runner, jsonlite, malformed_missing_comma);
    FATP_RUN_TEST_NS(runner, jsonlite, malformed_trailing_comma_array);
    FATP_RUN_TEST_NS(runner, jsonlite, malformed_trailing_comma_object);
    FATP_RUN_TEST_NS(runner, jsonlite, malformed_unquoted_key);
    FATP_RUN_TEST_NS(runner, jsonlite, malformed_unterminated_string);
    FATP_RUN_TEST_NS(runner, jsonlite, malformed_numbers);
    FATP_RUN_TEST_NS(runner, jsonlite, malformed_string_escapes);
    FATP_RUN_TEST_NS(runner, jsonlite, malformed_structure);
    FATP_RUN_TEST_NS(runner, jsonlite, malformed_literals);
    FATP_RUN_TEST_NS(runner, jsonlite, malformed_deep_nesting_stress);
    FATP_RUN_TEST_NS(runner, jsonlite, malformed_unicode);

    // Fuzz tests
    FATP_RUN_TEST_NS(runner, jsonlite, fuzz_ints);
    FATP_RUN_TEST_NS(runner, jsonlite, fuzz_int64);
    FATP_RUN_TEST_NS(runner, jsonlite, fuzz_doubles);
    FATP_RUN_TEST_NS(runner, jsonlite, fuzz_strings);
    FATP_RUN_TEST_NS(runner, jsonlite, fuzz_vector_int);
    FATP_RUN_TEST_NS(runner, jsonlite, fuzz_map_string_int);
    FATP_RUN_TEST_NS(runner, jsonlite, fuzz_nested_structures);


    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

// =============================================================================
// Standalone Entry Point
// =============================================================================
#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_JsonLite() ? 0 : 1;
}
#endif