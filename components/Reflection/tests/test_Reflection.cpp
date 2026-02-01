/**
 * @file test_Reflection.cpp
 * @brief Comprehensive unit tests for Reflection.h
 */
/*
FATP_META:
  meta_version: 1
  component: Reflection
  file_role: test
  path: components/Reflection/tests/test_Reflection.cpp
  layer: Testing
  namespace: fat_p::testing::reflection
  summary: "Unit tests for Reflection."
  api_stability: in_work
  related:
    docs_search: "Reflection"
    headers:
      - include/fat_p/Reflection.h
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

#include <iostream>
#include <string>
#include <vector>

#include "FatPTest.h"
#include "Reflection.h"

// ============================================================================
// Define Test Types (inside namespace)
// ============================================================================

namespace fat_p::testing::reflection
{

struct Point
{
    int x;
    int y;
};

FATP_REFLECT_DECLARE(Point, x, y); // Optional documentation

struct Person
{
    std::string name;
    int age;
    double height;
};

FATP_REFLECT_DECLARE(Person, name, age, height);

struct ComplexStruct
{
    int a;
    double b;
    std::string c;
    bool d;
    float e;
    long f;
    short g;
    char h;
};

FATP_REFLECT_DECLARE(ComplexStruct, a, b, c, d, e, f, g, h);

struct NestedStruct
{
    Point position;
    std::string label;
    int id;
};

FATP_REFLECT_DECLARE(NestedStruct, position, label, id);

} // namespace fat_p::testing::reflection

// ============================================================================
// Register Types for Reflection (at global scope)
// ============================================================================

// Unified syntax for both C++17 and C++20!
FATP_REFLECT_REGISTER(fat_p::testing::reflection::Point, x, y)
FATP_REFLECT_REGISTER(fat_p::testing::reflection::Person, name, age, height)
FATP_REFLECT_REGISTER(fat_p::testing::reflection::ComplexStruct, a, b, c, d, e, f, g, h)
FATP_REFLECT_REGISTER(fat_p::testing::reflection::NestedStruct, position, label, id)

// ============================================================================
// Test Implementation (back in namespace)
// ============================================================================

namespace fat_p::testing::reflection
{

// ============================================================================
// Unit Tests
// ============================================================================

FATP_TEST_CASE(basic_field_count)
{
    FATP_ASSERT_EQ(field_count<Point>(), 2u, "Point should have 2 fields");
    FATP_ASSERT_EQ(field_count<Person>(), 3u, "Person should have 3 fields");
    FATP_ASSERT_EQ(field_count<ComplexStruct>(), 8u, "ComplexStruct should have 8 fields");
    return true;
}

FATP_TEST_CASE(field_access_by_index)
{
    Point p{10, 20};

    FATP_ASSERT_EQ(get_field<0>(p), 10, "Field 0 should be x=10");
    FATP_ASSERT_EQ(get_field<1>(p), 20, "Field 1 should be y=20");

    // Test modification
    get_field<0>(p) = 100;
    get_field<1>(p) = 200;

    FATP_ASSERT_EQ(p.x, 100, "x should be modified to 100");
    FATP_ASSERT_EQ(p.y, 200, "y should be modified to 200");

    return true;
}

FATP_TEST_CASE(field_names)
{
    auto x_name = get_field_name<0, Point>();
    auto y_name = get_field_name<1, Point>();

    FATP_ASSERT_TRUE(x_name == "x", "Field 0 name should be x");
    FATP_ASSERT_TRUE(y_name == "y", "Field 1 name should be y");

    auto name_field = get_field_name<0, Person>();
    auto age_field = get_field_name<1, Person>();
    auto height_field = get_field_name<2, Person>();

    FATP_ASSERT_TRUE(name_field == "name", "Field 0 should be name");
    FATP_ASSERT_TRUE(age_field == "age", "Field 1 should be age");
    FATP_ASSERT_TRUE(height_field == "height", "Field 2 should be height");

    return true;
}

FATP_TEST_CASE(visit_fields)
{
    Person p{"Alice", 30, 165.5};

    std::vector<std::string> field_names;
    std::vector<std::string> field_values;

    visit_fields(p, [&](std::string_view name, const auto& value) {
        field_names.push_back(std::string(name));

        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, std::string>)
        {
            field_values.push_back(value);
        }
        else if constexpr (std::is_integral_v<T>)
        {
            field_values.push_back(std::to_string(value));
        }
        else if constexpr (std::is_floating_point_v<T>)
        {
            field_values.push_back(std::to_string(value));
        }
    });

    FATP_ASSERT_EQ(field_names.size(), 3u, "Should visit 3 fields");
    FATP_ASSERT_TRUE(field_names[0] == "name", "First field should be name");
    FATP_ASSERT_TRUE(field_names[1] == "age", "Second field should be age");
    FATP_ASSERT_TRUE(field_names[2] == "height", "Third field should be height");

    FATP_ASSERT_TRUE(field_values[0] == "Alice", "name value should be Alice");
    FATP_ASSERT_TRUE(field_values[1] == "30", "age value should be 30");

    return true;
}

FATP_TEST_CASE(field_accessor)
{
    Person p{"Bob", 25, 180.0};

    // Test has_field with O(N) lookup in C++17
    FATP_ASSERT_TRUE((FieldAccessor<Person>::has_field("name")), "Should have name field");
    FATP_ASSERT_TRUE((FieldAccessor<Person>::has_field("age")), "Should have age field");
    FATP_ASSERT_TRUE((FieldAccessor<Person>::has_field("height")), "Should have height field");
    FATP_ASSERT_TRUE(!(FieldAccessor<Person>::has_field("weight")), "Should not have weight field");

    // Test visit_field by name
    bool found_name = false;
    std::string retrieved_name;

    FieldAccessor<Person>::visit_field(p, "name", [&](const auto& value) {
        found_name = true;
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, std::string>)
        {
            retrieved_name = value;
        }
    });

    FATP_ASSERT_TRUE(found_name, "Should find name field");
    FATP_ASSERT_TRUE(retrieved_name == "Bob", "Retrieved name should be Bob");

    // Test modification via visit_field
    FieldAccessor<Person>::visit_field(p, "age", [](auto& value) {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_integral_v<T>)
        {
            value = 26;
        }
    });

    FATP_ASSERT_EQ(p.age, 26, "Age should be modified to 26");

    return true;
}

FATP_TEST_CASE(hash_lookup_performance)
{
    // Test that lookup works correctly
    Person p{"Charlie", 35, 175.0};

    // Test multiple lookups
    for (const char* field : {"name", "age", "height"})
    {
        FATP_ASSERT_TRUE(FieldAccessor<Person>::has_field(field), "Lookup should find field");
    }

    // Test non-existent field
    FATP_ASSERT_TRUE(!FieldAccessor<Person>::has_field("nonexistent"), "Lookup should not find non-existent field");

    return true;
}

FATP_TEST_CASE(to_tuple)
{
    Point p{5, 10};
    auto tuple = to_tuple(p);

    FATP_ASSERT_EQ(std::get<0>(tuple), 5, "Tuple element 0 should be 5");
    FATP_ASSERT_EQ(std::get<1>(tuple), 10, "Tuple element 1 should be 10");

    // Modify through tuple
    std::get<0>(tuple) = 50;
    std::get<1>(tuple) = 100;

    FATP_ASSERT_EQ(p.x, 50, "Point.x should be modified via tuple");
    FATP_ASSERT_EQ(p.y, 100, "Point.y should be modified via tuple");

    return true;
}

FATP_TEST_CASE(type_name)
{
    auto point_name = type_name<Point>();
    auto person_name = type_name<Person>();
    auto int_name = type_name<int>();
    auto double_name = type_name<double>();

    // Just verify that we get some non-empty type names
    FATP_ASSERT_TRUE(!point_name.empty(), "Point type name should not be empty");
    FATP_ASSERT_TRUE(!person_name.empty(), "Person type name should not be empty");
    FATP_ASSERT_TRUE(!int_name.empty(), "int type name should not be empty");
    FATP_ASSERT_TRUE(!double_name.empty(), "double type name should not be empty");

    // The actual names depend on compiler, but should contain the type name
    FATP_ASSERT_TRUE(point_name.find("Point") != std::string_view::npos, "Point type name should contain Point");

    return true;
}

FATP_TEST_CASE(is_reflectable)
{
    FATP_ASSERT_TRUE(is_reflectable<Point>(), "Point should be reflectable");
    FATP_ASSERT_TRUE(is_reflectable<Person>(), "Person should be reflectable");
    FATP_ASSERT_TRUE(is_reflectable<ComplexStruct>(), "ComplexStruct should be reflectable");
    FATP_ASSERT_TRUE(!is_reflectable<int>(), "int should not be reflectable");
    FATP_ASSERT_TRUE(!is_reflectable<std::string>(), "std::string should not be reflectable");

    return true;
}

FATP_TEST_CASE(complex_types)
{
    ComplexStruct obj{
        42,         // int a
        3.14,       // double b
        "hello",    // string c
        true,       // bool d
        2.71f,      // float e
        123456789L, // long f
        100,        // short g
        'A'         // char h
    };

    FATP_ASSERT_EQ(field_count<ComplexStruct>(), 8u, "Should have 8 fields");

    // Test accessing all fields
    FATP_ASSERT_EQ(get_field<0>(obj), 42, "Field a should be 42");
    FATP_ASSERT_EQ(get_field<1>(obj), 3.14, "Field b should be 3.14");

    auto c_val = get_field<2>(obj);
    FATP_ASSERT_TRUE(c_val == "hello", "Field c should be hello");

    FATP_ASSERT_EQ(get_field<3>(obj), true, "Field d should be true");
    FATP_ASSERT_EQ(get_field<4>(obj), 2.71f, "Field e should be 2.71f");
    FATP_ASSERT_EQ(get_field<5>(obj), 123456789L, "Field f should be 123456789L");
    FATP_ASSERT_EQ(get_field<6>(obj), short(100), "Field g should be 100");
    FATP_ASSERT_EQ(get_field<7>(obj), 'A', "Field h should be A");

    // Test field names
    auto name0 = get_field_name<0, ComplexStruct>();
    auto name1 = get_field_name<1, ComplexStruct>();
    auto name7 = get_field_name<7, ComplexStruct>();

    FATP_ASSERT_TRUE(name0 == "a", "Field 0 name should be a");
    FATP_ASSERT_TRUE(name1 == "b", "Field 1 name should be b");
    FATP_ASSERT_TRUE(name7 == "h", "Field 7 name should be h");

    return true;
}

FATP_TEST_CASE(get_field_names)
{
    auto names = get_field_names<Point>();

    FATP_ASSERT_EQ(names.size(), 2u, "Should have 2 field names");
    FATP_ASSERT_TRUE(names[0] == "x", "First name should be x");
    FATP_ASSERT_TRUE(names[1] == "y", "Second name should be y");

    auto person_names = get_field_names<Person>();
    FATP_ASSERT_EQ(person_names.size(), 3u, "Should have 3 field names");
    FATP_ASSERT_TRUE(person_names[0] == "name", "First name should be name");
    FATP_ASSERT_TRUE(person_names[1] == "age", "Second name should be age");
    FATP_ASSERT_TRUE(person_names[2] == "height", "Third name should be height");

    return true;
}

FATP_TEST_CASE(to_debug_string)
{
    Point p{42, 84};
    std::string debug = to_debug_string(p);

    // Verify the debug string contains expected information
    FATP_ASSERT_TRUE(debug.find("Point") != std::string::npos, "Debug string should contain type name Point");
    FATP_ASSERT_TRUE(debug.find("x") != std::string::npos, "Debug string should contain field name x");
    FATP_ASSERT_TRUE(debug.find("y") != std::string::npos, "Debug string should contain field name y");
    FATP_ASSERT_TRUE(debug.find("42") != std::string::npos, "Debug string should contain value 42");
    FATP_ASSERT_TRUE(debug.find("84") != std::string::npos, "Debug string should contain value 84");

    Person person{"Charlie", 35, 175.0};
    std::string person_debug = to_debug_string(person);

    FATP_ASSERT_TRUE(person_debug.find("Person") != std::string::npos, "Debug string should contain Person");
    FATP_ASSERT_TRUE(person_debug.find("Charlie") != std::string::npos, "Debug string should contain Charlie");
    FATP_ASSERT_TRUE(person_debug.find("35") != std::string::npos, "Debug string should contain age 35");

    return true;
}

FATP_TEST_CASE(const_access)
{
    const Point p{7, 14};

    FATP_ASSERT_EQ(get_field<0>(p), 7, "Const access to field 0 should work");
    FATP_ASSERT_EQ(get_field<1>(p), 14, "Const access to field 1 should work");

    const Person person{"Diana", 28, 168.5};

    bool visited = false;
    visit_fields(person, [&](std::string_view, const auto&) {
        visited = true;
    });

    FATP_ASSERT_TRUE(visited, "Should be able to visit const object fields");

    return true;
}

FATP_TEST_CASE(nested_structs)
{
    NestedStruct ns{{10, 20}, "Origin", 1};

    FATP_ASSERT_EQ(field_count<NestedStruct>(), 3u, "NestedStruct should have 3 fields");

    auto& pos = get_field<0>(ns);
    FATP_ASSERT_EQ(pos.x, 10, "Nested Point.x should be 10");
    FATP_ASSERT_EQ(pos.y, 20, "Nested Point.y should be 20");

    auto label = get_field<1>(ns);
    FATP_ASSERT_TRUE(label == "Origin", "label should be Origin");
    FATP_ASSERT_EQ(get_field<2>(ns), 1, "id should be 1");

    // Modify nested struct
    get_field<0>(ns).x = 100;
    FATP_ASSERT_EQ(ns.position.x, 100, "Should modify nested field");

    return true;
}

// ============================================================================
// Benchmarks
// ============================================================================
// ============================================================================
// Main Test Function
// ============================================================================

} // namespace fat_p::testing::reflection

namespace fat_p::testing
{


void run_benchmarks()
{
    using namespace fat_p::testing;

    auto& out = *get_test_config().output;
    out << "\n" << colors::cyan() << "Sanity Benchmark:" << colors::reset() << "\n";
    out << "  (benchmarks are provided by benchmark executables; unit tests do not run full benchmarks)\n\n";
}

bool test_Reflection()
{
    FATP_PRINT_HEADER(REFLECTION)

    std::cout << "Unified FATP_REFLECT_REGISTER Syntax with NTTP Field Names\n";

    TestRunner runner;

    FATP_RUN_TEST_NS(runner, reflection, basic_field_count);
    FATP_RUN_TEST_NS(runner, reflection, field_access_by_index);
    FATP_RUN_TEST_NS(runner, reflection, field_names);
    FATP_RUN_TEST_NS(runner, reflection, visit_fields);
    FATP_RUN_TEST_NS(runner, reflection, field_accessor);
    FATP_RUN_TEST_NS(runner, reflection, hash_lookup_performance);
    FATP_RUN_TEST_NS(runner, reflection, to_tuple);
    FATP_RUN_TEST_NS(runner, reflection, type_name);
    FATP_RUN_TEST_NS(runner, reflection, is_reflectable);
    FATP_RUN_TEST_NS(runner, reflection, complex_types);
    FATP_RUN_TEST_NS(runner, reflection, get_field_names);
    FATP_RUN_TEST_NS(runner, reflection, to_debug_string);
    FATP_RUN_TEST_NS(runner, reflection, const_access);
    FATP_RUN_TEST_NS(runner, reflection, nested_structs);


    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

// =============================================================================
// Standalone Entry Point
// =============================================================================
#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_Reflection() ? 0 : 1;
}
#endif
