#include <iostream>
#include <string>
#include <vector>

#include "Reflection.h"
#include "test_Reflection.h"
#include "FatPTest.h"

// ============================================================================
// Define Test Types (inside namespace)
// ============================================================================

namespace fat_p::testing
{

struct Point {
    int x;
    int y;
};

REFLECT_DECLARE(Point, x, y);  // Optional documentation

struct Person {
    std::string name;
    int age;
    double height;
};

REFLECT_DECLARE(Person, name, age, height);

struct ComplexStruct {
    int a;
    double b;
    std::string c;
    bool d;
    float e;
    long f;
    short g;
    char h;
};

REFLECT_DECLARE(ComplexStruct, a, b, c, d, e, f, g, h);

struct NestedStruct {
    Point position;
    std::string label;
    int id;
};

REFLECT_DECLARE(NestedStruct, position, label, id);

} // namespace fat_p::testing

// ============================================================================
// Register Types for Reflection (at global scope)
// ============================================================================

// Unified syntax for both C++17 and C++20!
REFLECT_REGISTER(fat_p::testing::Point, x, y)
REFLECT_REGISTER(fat_p::testing::Person, name, age, height)
REFLECT_REGISTER(fat_p::testing::ComplexStruct, a, b, c, d, e, f, g, h)
REFLECT_REGISTER(fat_p::testing::NestedStruct, position, label, id)

// ============================================================================
// Test Implementation (back in namespace)
// ============================================================================

namespace fat_p::testing
{

using namespace fat_p;

// ============================================================================
// Unit Tests
// ============================================================================

bool test_reflection_basic_field_count() {
    ASSERT_EQ(field_count<Point>(), 2u, "Point should have 2 fields");
    ASSERT_EQ(field_count<Person>(), 3u, "Person should have 3 fields");
    ASSERT_EQ(field_count<ComplexStruct>(), 8u, "ComplexStruct should have 8 fields");
    return true;
}

bool test_reflection_field_access_by_index() {
    Point p{10, 20};
    
    ASSERT_EQ(get_field<0>(p), 10, "Field 0 should be x=10");
    ASSERT_EQ(get_field<1>(p), 20, "Field 1 should be y=20");
    
    // Test modification
    get_field<0>(p) = 100;
    get_field<1>(p) = 200;
    
    ASSERT_EQ(p.x, 100, "x should be modified to 100");
    ASSERT_EQ(p.y, 200, "y should be modified to 200");
    
    return true;
}

bool test_reflection_field_names() {
    auto x_name = get_field_name<0, Point>();
    auto y_name = get_field_name<1, Point>();
    
    SIMPLE_ASSERT(x_name == "x", "Field 0 name should be x");
    SIMPLE_ASSERT(y_name == "y", "Field 1 name should be y");
    
    auto name_field = get_field_name<0, Person>();
    auto age_field = get_field_name<1, Person>();
    auto height_field = get_field_name<2, Person>();
    
    SIMPLE_ASSERT(name_field == "name", "Field 0 should be name");
    SIMPLE_ASSERT(age_field == "age", "Field 1 should be age");
    SIMPLE_ASSERT(height_field == "height", "Field 2 should be height");
    
    return true;
}

bool test_reflection_visit_fields() {
    Person p{"Alice", 30, 165.5};
    
    std::vector<std::string> field_names;
    std::vector<std::string> field_values;
    
    visit_fields(p, [&](std::string_view name, const auto& value) {
        field_names.push_back(std::string(name));
        
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, std::string>) {
            field_values.push_back(value);
        } else if constexpr (std::is_integral_v<T>) {
            field_values.push_back(std::to_string(value));
        } else if constexpr (std::is_floating_point_v<T>) {
            field_values.push_back(std::to_string(value));
        }
    });
    
    ASSERT_EQ(field_names.size(), 3u, "Should visit 3 fields");
    SIMPLE_ASSERT(field_names[0] == "name", "First field should be name");
    SIMPLE_ASSERT(field_names[1] == "age", "Second field should be age");
    SIMPLE_ASSERT(field_names[2] == "height", "Third field should be height");
    
    SIMPLE_ASSERT(field_values[0] == "Alice", "name value should be Alice");
    SIMPLE_ASSERT(field_values[1] == "30", "age value should be 30");
    
    return true;
}

bool test_reflection_field_accessor() {
    Person p{"Bob", 25, 180.0};
    
    // Test has_field with O(N) lookup in C++17
    SIMPLE_ASSERT((FieldAccessor<Person>::has_field("name")), "Should have name field");
    SIMPLE_ASSERT((FieldAccessor<Person>::has_field("age")), "Should have age field");
    SIMPLE_ASSERT((FieldAccessor<Person>::has_field("height")), "Should have height field");
    SIMPLE_ASSERT(!(FieldAccessor<Person>::has_field("weight")), "Should not have weight field");
    
    // Test visit_field by name
    bool found_name = false;
    std::string retrieved_name;
    
    FieldAccessor<Person>::visit_field(p, "name", [&](const auto& value) {
        found_name = true;
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, std::string>) {
            retrieved_name = value;
        }
    });
    
    SIMPLE_ASSERT(found_name, "Should find name field");
    SIMPLE_ASSERT(retrieved_name == "Bob", "Retrieved name should be Bob");
    
    // Test modification via visit_field
    FieldAccessor<Person>::visit_field(p, "age", [](auto& value) {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_integral_v<T>) {
            value = 26;
        }
    });
    
    ASSERT_EQ(p.age, 26, "Age should be modified to 26");
    
    return true;
}

bool test_reflection_hash_lookup_performance() {
    // Test that lookup works correctly
    Person p{"Charlie", 35, 175.0};
    
    // Test multiple lookups
    for (const char* field : {"name", "age", "height"}) {
        SIMPLE_ASSERT(FieldAccessor<Person>::has_field(field), 
                     "Lookup should find field");
    }
    
    // Test non-existent field
    SIMPLE_ASSERT(!FieldAccessor<Person>::has_field("nonexistent"), 
                 "Lookup should not find non-existent field");
    
    return true;
}

bool test_reflection_to_tuple() {
    Point p{5, 10};
    auto tuple = to_tuple(p);
    
    ASSERT_EQ(std::get<0>(tuple), 5, "Tuple element 0 should be 5");
    ASSERT_EQ(std::get<1>(tuple), 10, "Tuple element 1 should be 10");
    
    // Modify through tuple
    std::get<0>(tuple) = 50;
    std::get<1>(tuple) = 100;
    
    ASSERT_EQ(p.x, 50, "Point.x should be modified via tuple");
    ASSERT_EQ(p.y, 100, "Point.y should be modified via tuple");
    
    return true;
}

bool test_reflection_type_name() {
    auto point_name = type_name<Point>();
    auto person_name = type_name<Person>();
    auto int_name = type_name<int>();
    auto double_name = type_name<double>();
    
    // Just verify that we get some non-empty type names
    SIMPLE_ASSERT(!point_name.empty(), "Point type name should not be empty");
    SIMPLE_ASSERT(!person_name.empty(), "Person type name should not be empty");
    SIMPLE_ASSERT(!int_name.empty(), "int type name should not be empty");
    SIMPLE_ASSERT(!double_name.empty(), "double type name should not be empty");
    
    // The actual names depend on compiler, but should contain the type name
    SIMPLE_ASSERT(point_name.find("Point") != std::string_view::npos, 
                  "Point type name should contain Point");
    
    return true;
}

bool test_reflection_is_reflectable() {
    SIMPLE_ASSERT(is_reflectable<Point>(), "Point should be reflectable");
    SIMPLE_ASSERT(is_reflectable<Person>(), "Person should be reflectable");
    SIMPLE_ASSERT(is_reflectable<ComplexStruct>(), "ComplexStruct should be reflectable");
    SIMPLE_ASSERT(!is_reflectable<int>(), "int should not be reflectable");
    SIMPLE_ASSERT(!is_reflectable<std::string>(), "std::string should not be reflectable");
    
    return true;
}

bool test_reflection_complex_types() {
    ComplexStruct obj{
        42,           // int a
        3.14,         // double b
        "hello",      // string c
        true,         // bool d
        2.71f,        // float e
        123456789L,   // long f
        100,          // short g
        'A'           // char h
    };
    
    ASSERT_EQ(field_count<ComplexStruct>(), 8u, "Should have 8 fields");
    
    // Test accessing all fields
    ASSERT_EQ(get_field<0>(obj), 42, "Field a should be 42");
    ASSERT_EQ(get_field<1>(obj), 3.14, "Field b should be 3.14");
    
    auto c_val = get_field<2>(obj);
    SIMPLE_ASSERT(c_val == "hello", "Field c should be hello");
    
    ASSERT_EQ(get_field<3>(obj), true, "Field d should be true");
    ASSERT_EQ(get_field<4>(obj), 2.71f, "Field e should be 2.71f");
    ASSERT_EQ(get_field<5>(obj), 123456789L, "Field f should be 123456789L");
    ASSERT_EQ(get_field<6>(obj), short(100), "Field g should be 100");
    ASSERT_EQ(get_field<7>(obj), 'A', "Field h should be A");
    
    // Test field names
    auto name0 = get_field_name<0, ComplexStruct>();
    auto name1 = get_field_name<1, ComplexStruct>();
    auto name7 = get_field_name<7, ComplexStruct>();
    
    SIMPLE_ASSERT(name0 == "a", "Field 0 name should be a");
    SIMPLE_ASSERT(name1 == "b", "Field 1 name should be b");
    SIMPLE_ASSERT(name7 == "h", "Field 7 name should be h");
    
    return true;
}

bool test_reflection_get_field_names() {
    auto names = get_field_names<Point>();
    
    ASSERT_EQ(names.size(), 2u, "Should have 2 field names");
    SIMPLE_ASSERT(names[0] == "x", "First name should be x");
    SIMPLE_ASSERT(names[1] == "y", "Second name should be y");
    
    auto person_names = get_field_names<Person>();
    ASSERT_EQ(person_names.size(), 3u, "Should have 3 field names");
    SIMPLE_ASSERT(person_names[0] == "name", "First name should be name");
    SIMPLE_ASSERT(person_names[1] == "age", "Second name should be age");
    SIMPLE_ASSERT(person_names[2] == "height", "Third name should be height");
    
    return true;
}

bool test_reflection_to_debug_string() {
    Point p{42, 84};
    std::string debug = to_debug_string(p);
    
    // Verify the debug string contains expected information
    SIMPLE_ASSERT(debug.find("Point") != std::string::npos, 
                  "Debug string should contain type name Point");
    SIMPLE_ASSERT(debug.find("x") != std::string::npos, 
                  "Debug string should contain field name x");
    SIMPLE_ASSERT(debug.find("y") != std::string::npos, 
                  "Debug string should contain field name y");
    SIMPLE_ASSERT(debug.find("42") != std::string::npos, 
                  "Debug string should contain value 42");
    SIMPLE_ASSERT(debug.find("84") != std::string::npos, 
                  "Debug string should contain value 84");
    
    Person person{"Charlie", 35, 175.0};
    std::string person_debug = to_debug_string(person);
    
    SIMPLE_ASSERT(person_debug.find("Person") != std::string::npos, 
                  "Debug string should contain Person");
    SIMPLE_ASSERT(person_debug.find("Charlie") != std::string::npos, 
                  "Debug string should contain Charlie");
    SIMPLE_ASSERT(person_debug.find("35") != std::string::npos, 
                  "Debug string should contain age 35");
    
    return true;
}

bool test_reflection_const_access() {
    const Point p{7, 14};
    
    ASSERT_EQ(get_field<0>(p), 7, "Const access to field 0 should work");
    ASSERT_EQ(get_field<1>(p), 14, "Const access to field 1 should work");
    
    const Person person{"Diana", 28, 168.5};
    
    bool visited = false;
    visit_fields(person, [&](std::string_view, const auto&) {
        visited = true;
    });
    
    SIMPLE_ASSERT(visited, "Should be able to visit const object fields");
    
    return true;
}

bool test_reflection_nested_structs() {
    NestedStruct ns{{10, 20}, "Origin", 1};
    
    ASSERT_EQ(field_count<NestedStruct>(), 3u, "NestedStruct should have 3 fields");
    
    auto& pos = get_field<0>(ns);
    ASSERT_EQ(pos.x, 10, "Nested Point.x should be 10");
    ASSERT_EQ(pos.y, 20, "Nested Point.y should be 20");
    
    auto label = get_field<1>(ns);
    SIMPLE_ASSERT(label == "Origin", "label should be Origin");
    ASSERT_EQ(get_field<2>(ns), 1, "id should be 1");
    
    // Modify nested struct
    get_field<0>(ns).x = 100;
    ASSERT_EQ(ns.position.x, 100, "Should modify nested field");
    
    return true;
}

// ============================================================================
// Benchmarks
// ============================================================================

void benchmark_reflection() {
    std::cout << "\n" << colors::cyan() << "Reflection Benchmarks:" << colors::reset() << "\n\n";
    
    Point p{0, 0};
    
    // Benchmark direct field access
    double direct_time = measure_perf([&p, i=0]() mutable {
        p.x = i++;
        DoNotOptimize(p.x);
    }, 100000, 1000);
    std::cout << "Direct field access: " << format_time(direct_time) << "\n";
    
    // Benchmark reflection field access
    double reflect_time = measure_perf([&p, i=0]() mutable {
        get_field<0>(p) = i++;
        DoNotOptimize(get_field<0>(p));
    }, 100000, 1000);
    std::cout << "Reflection field access: " << format_time(reflect_time) << "\n";
    
    // Benchmark visit_fields
    Person person{"Test", 0, 0.0};
    double visit_time = measure_perf([&person]() {
        visit_fields(person, [](std::string_view, const auto& value) {
            DoNotOptimize(value);
        });
    }, 10000, 100);
    std::cout << "visit_fields (3 fields): " << format_time(visit_time) << "\n";
    
    // Benchmark field lookup by name
    double lookup_time = measure_perf([&person]() {
        FieldAccessor<Person>::visit_field(person, "age", [](const auto& value) {
            DoNotOptimize(value);
        });
    }, 10000, 100);
    std::cout << "Field lookup by name (linear O(N)): " << format_time(lookup_time) << "\n";
    
    // Benchmark to_tuple
    double tuple_time = measure_perf([&p]() {
        auto t = to_tuple(p);
        DoNotOptimize(t);
    }, 10000, 100);
    std::cout << "to_tuple conversion: " << format_time(tuple_time) << "\n";
    
    // Benchmark get_field_names
    double names_time = measure_perf([]() {
        auto names = get_field_names<Person>();
        DoNotOptimize(names);
    }, 10000, 100);
    std::cout << "get_field_names: " << format_time(names_time) << "\n";
}

// ============================================================================
// Main Test Function
// ============================================================================

bool test_Reflection() {

    PRINT_HEADER(REFLECTION)

#if FATP_HAS_CPP20
    std::cout << "(C++20 Mode) Unified REFLECT_REGISTER Syntax with NTTP Field Names\n";
#else
    std::cout << "(C++17 Mode) Unified REFLECT_REGISTER Syntax with Constructor Field Names\n";
#endif

    TestRunner runner;

    RUN_TEST(runner, reflection_basic_field_count);
    RUN_TEST(runner, reflection_field_access_by_index);
    RUN_TEST(runner, reflection_field_names);
    RUN_TEST(runner, reflection_visit_fields);
    RUN_TEST(runner, reflection_field_accessor);
    RUN_TEST(runner, reflection_hash_lookup_performance);
    RUN_TEST(runner, reflection_to_tuple);
    RUN_TEST(runner, reflection_type_name);
    RUN_TEST(runner, reflection_is_reflectable);
    RUN_TEST(runner, reflection_complex_types);
    RUN_TEST(runner, reflection_get_field_names);
    RUN_TEST(runner, reflection_to_debug_string);
    RUN_TEST(runner, reflection_const_access);
    RUN_TEST(runner, reflection_nested_structs);

    benchmark_reflection();

    return 0 == runner.print_summary();
}

} // namespace fat_p::testing
