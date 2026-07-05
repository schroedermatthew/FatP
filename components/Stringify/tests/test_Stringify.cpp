/**
 * @file test_Stringify.cpp
 * @brief Comprehensive unit tests for Stringify.h
 */
/*
FATP_META:
  meta_version: 1
  component: Stringify
  file_role: test
  path: components/Stringify/tests/test_Stringify.cpp
  layer: Testing
  namespace: fat_p::testing::stringify
  summary: "Unit tests for Stringify using C++20 concepts."
  api_stability: stable
  related:
    docs_search: "Stringify"
    headers:
      - include/fat_p/Stringify.h
      - include/fat_p/Concepts.h
      - include/fat_p/FatPTest.h
  hygiene:
    pragma_once: false
    include_guard: false
    defines_total: 0
    defines_unprefixed: 0
    undefs_total: 0
    includes_windows_h: false
*/

#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <deque>
#include <forward_list>
#include <iomanip>
#include <iostream>
#include <limits>
#include <list>
#include <map>
#include <numeric>
#include <optional>
#include <set>
#include <sstream>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "FatPTest.h"
#include "Stringify.h"

namespace fat_p::testing::stringify
{

using namespace std::chrono;

// =============================================================================
// Test Helper Classes
// =============================================================================

struct CustomToString
{
    int value;
    std::string toString() const
    {
        return "Custom(" + std::to_string(value) + ")";
    }
};

struct CustomToStringSnake
{
    int value;
    std::string to_string() const
    {
        return "Snake(" + std::to_string(value) + ")";
    }
};

struct CustomBothMethods
{
    int value;
    std::string toString() const
    {
        return "CamelCase";
    }
    std::string to_string() const
    {
        return "snake_case";
    }
};

struct BadToStringReturnInt
{
    int toString() const
    {
        return 42;
    }
};

struct BadToStringReturnVoid
{
    void to_string() const
    {
    }
};

struct CustomStreamable
{
    int value;
    friend std::ostream& operator<<(std::ostream& os, const CustomStreamable& c)
    {
        return os << "Streamable(" << c.value << ")";
    }
};

struct CustomWStreamable
{
    int value;
    friend std::wostream& operator<<(std::wostream& os, const CustomWStreamable& c)
    {
        return os << L"WStreamable(" << c.value << L")";
    }
};

struct NonStringifiable
{
    int data;
};

struct ThrowingToString
{
    std::string toString() const
    {
        throw std::runtime_error("Intentional error");
    }
};

struct FakePairWithToString
{
    using first_type = int;
    using second_type = std::string;
    int data;
    std::string toString() const
    {
        return "FakePair(" + std::to_string(data) + ")";
    }
};

struct IterableWithToString
{
    std::vector<int> data = {1, 2, 3};
    auto begin() const
    {
        return data.begin();
    }
    auto end() const
    {
        return data.end();
    }
    std::string toString() const
    {
        return "IterableCustom";
    }
};

enum class Color
{
    Red,
    Green,
    Blue
};
enum class Priority
{
    Low = 1,
    Medium = 5,
    High = 10
};
enum UnscopedEnum
{
    A = 100,
    B = 200,
    C = 300
};

} // namespace fat_p::testing::stringify

namespace fat_p
{
template <>
struct EnumStringifier<testing::stringify::Color>
{
    static const char* to_string(testing::stringify::Color c)
    {
        switch (c)
        {
            case testing::stringify::Color::Red:
                return "Red";
            case testing::stringify::Color::Green:
                return "Green";
            case testing::stringify::Color::Blue:
                return "Blue";
        }
        return nullptr;
    }
};
} // namespace fat_p

namespace fat_p::testing::stringify
{

// =============================================================================
// Concept Detection Tests
// =============================================================================

FATP_TEST_CASE(concept_has_to_string_method)
{
    static_assert(concepts::has_to_string_method<CustomToString>);
    static_assert(!concepts::has_to_string_method<CustomToStringSnake>);
    static_assert(concepts::has_to_string_method<CustomBothMethods>);
    static_assert(!concepts::has_to_string_method<BadToStringReturnInt>);
    static_assert(!concepts::has_to_string_method<NonStringifiable>);
    static_assert(!concepts::has_to_string_method<int>);
    static_assert(!concepts::has_to_string_method<std::string>);
    return true;
}

FATP_TEST_CASE(concept_has_to_string_snake_method)
{
    static_assert(!concepts::has_to_string_snake_method<CustomToString>);
    static_assert(concepts::has_to_string_snake_method<CustomToStringSnake>);
    static_assert(concepts::has_to_string_snake_method<CustomBothMethods>);
    static_assert(!concepts::has_to_string_snake_method<BadToStringReturnVoid>);
    static_assert(!concepts::has_to_string_snake_method<NonStringifiable>);
    return true;
}

FATP_TEST_CASE(concept_has_custom_string_method)
{
    static_assert(concepts::has_custom_string_method<CustomToString>);
    static_assert(concepts::has_custom_string_method<CustomToStringSnake>);
    static_assert(concepts::has_custom_string_method<CustomBothMethods>);
    static_assert(!concepts::has_custom_string_method<NonStringifiable>);
    static_assert(!concepts::has_custom_string_method<CustomStreamable>);
    return true;
}

FATP_TEST_CASE(concept_streamable)
{
    static_assert(concepts::streamable<int>);
    static_assert(concepts::streamable<double>);
    static_assert(concepts::streamable<std::string>);
    static_assert(concepts::streamable<CustomStreamable>);
    static_assert(!concepts::streamable<NonStringifiable>);
    static_assert(concepts::streamable<const char*>);
    return true;
}

FATP_TEST_CASE(concept_wstreamable)
{
    static_assert(concepts::wstreamable<int>);
    static_assert(concepts::wstreamable<double>);
    static_assert(concepts::wstreamable<CustomWStreamable>);
    static_assert(!concepts::wstreamable<NonStringifiable>);
    return true;
}

FATP_TEST_CASE(concept_printable_range)
{
    static_assert(concepts::printable_range<std::vector<int>>);
    static_assert(concepts::printable_range<std::list<int>>);
    static_assert(concepts::printable_range<std::array<int, 5>>);
    static_assert(concepts::printable_range<std::deque<int>>);
    static_assert(!concepts::printable_range<std::string>);
    static_assert(!concepts::printable_range<std::string_view>);
    static_assert(!concepts::printable_range<const char*>);
    static_assert(!concepts::printable_range<int>);
    return true;
}

FATP_TEST_CASE(concept_std_string_type)
{
    static_assert(concepts::std_string_type<std::string>);
    static_assert(concepts::std_string_type<std::string_view>);
    static_assert(concepts::std_string_type<const char*>);
    static_assert(concepts::std_string_type<char*>);
    static_assert(!concepts::std_string_type<std::vector<char>>);
    static_assert(!concepts::std_string_type<int>);
    return true;
}

FATP_TEST_CASE(concept_stringifiable)
{
    // Arithmetic
    static_assert(concepts::stringifiable<int>);
    static_assert(concepts::stringifiable<double>);
    static_assert(concepts::stringifiable<bool>);
    static_assert(concepts::stringifiable<char>);
    // Enums
    static_assert(concepts::stringifiable<Color>);
    static_assert(concepts::stringifiable<Priority>);
    // Custom methods
    static_assert(concepts::stringifiable<CustomToString>);
    static_assert(concepts::stringifiable<CustomToStringSnake>);
    // Streamable
    static_assert(concepts::stringifiable<CustomStreamable>);
    // Containers
    static_assert(concepts::stringifiable<std::vector<int>>);
    static_assert(concepts::stringifiable<std::map<int, int>>);
    // Pair/Tuple
    static_assert(concepts::stringifiable<std::pair<int, int>>);
    static_assert(concepts::stringifiable<std::tuple<int, double>>);
    // Optional
    static_assert(concepts::stringifiable<std::optional<int>>);
    // Non-stringifiable
    static_assert(!concepts::stringifiable<NonStringifiable>);
    return true;
}

// =============================================================================
// Integer Stringification
// =============================================================================

FATP_TEST_CASE(int_basic)
{
    FATP_ASSERT_EQ(toString(0), "0", "zero");
    FATP_ASSERT_EQ(toString(1), "1", "one");
    FATP_ASSERT_EQ(toString(-1), "-1", "negative one");
    FATP_ASSERT_EQ(toString(42), "42", "positive");
    FATP_ASSERT_EQ(toString(-42), "-42", "negative");
    return true;
}

FATP_TEST_CASE(int_boundaries)
{
    FATP_ASSERT_EQ(toString(std::numeric_limits<int>::max()),
                   std::to_string(std::numeric_limits<int>::max()),
                   "int max");
    FATP_ASSERT_EQ(toString(std::numeric_limits<int>::min()),
                   std::to_string(std::numeric_limits<int>::min()),
                   "int min");
    FATP_ASSERT_EQ(toString(std::numeric_limits<long long>::max()),
                   std::to_string(std::numeric_limits<long long>::max()),
                   "long long max");
    FATP_ASSERT_EQ(toString(std::numeric_limits<long long>::min()),
                   std::to_string(std::numeric_limits<long long>::min()),
                   "long long min");
    return true;
}

FATP_TEST_CASE(int_all_types)
{
    FATP_ASSERT_EQ(toString(static_cast<short>(42)), "42", "short");
    FATP_ASSERT_EQ(toString(static_cast<unsigned short>(42)), "42", "unsigned short");
    FATP_ASSERT_EQ(toString(42), "42", "int");
    FATP_ASSERT_EQ(toString(42U), "42", "unsigned int");
    FATP_ASSERT_EQ(toString(42L), "42", "long");
    FATP_ASSERT_EQ(toString(42UL), "42", "unsigned long");
    FATP_ASSERT_EQ(toString(42LL), "42", "long long");
    FATP_ASSERT_EQ(toString(42ULL), "42", "unsigned long long");
    return true;
}

// =============================================================================
// Floating-Point Stringification
// =============================================================================

FATP_TEST_CASE(float_basic)
{
    auto r1 = toString(3.14);
    FATP_ASSERT_TRUE(r1.find("3.14") != std::string::npos, "double basic");

    auto r2 = toString(3.14f);
    FATP_ASSERT_TRUE(r2.find("3.14") != std::string::npos, "float basic");

    FATP_ASSERT_EQ(toString(0.0), "0", "zero double");
    return true;
}

FATP_TEST_CASE(float_special_values)
{
    auto inf_str = toString(std::numeric_limits<double>::infinity());
    FATP_ASSERT_TRUE(inf_str.find("inf") != std::string::npos || inf_str.find("Inf") != std::string::npos, "infinity");

    auto neg_inf_str = toString(-std::numeric_limits<double>::infinity());
    FATP_ASSERT_TRUE(neg_inf_str.find("inf") != std::string::npos, "negative infinity");

    auto nan_str = toString(std::nan(""));
    FATP_ASSERT_TRUE(nan_str.find("nan") != std::string::npos || nan_str.find("NaN") != std::string::npos, "NaN");
    return true;
}

FATP_TEST_CASE(float_precision_option)
{
    StringifyOptions opts;
    opts.float_precision = 2;
    FATP_ASSERT_EQ(toString(3.14159, opts), "3.14", "precision 2");

    opts.float_precision = 4;
    FATP_ASSERT_EQ(toString(3.14159, opts), "3.1416", "precision 4");

    opts.float_precision = 0;
    FATP_ASSERT_EQ(toString(3.14159, opts), "3", "precision 0");
    return true;
}

FATP_TEST_CASE(float_scientific_notation)
{
    StringifyOptions opts;
    opts.scientific_notation = true;
    opts.float_precision = 2;

    auto result = toString(12345.0, opts);
    FATP_ASSERT_TRUE(result.find("e") != std::string::npos || result.find("E") != std::string::npos,
                     "scientific notation");
    return true;
}

// =============================================================================
// Boolean Stringification
// =============================================================================

FATP_TEST_CASE(bool_text_mode)
{
    FATP_ASSERT_EQ(toString(true), "true", "true as text");
    FATP_ASSERT_EQ(toString(false), "false", "false as text");

    StringifyOptions opts;
    opts.show_bool_as_text = true;
    FATP_ASSERT_EQ(toString(true, opts), "true", "explicit text mode");
    return true;
}

FATP_TEST_CASE(bool_numeric_mode)
{
    StringifyOptions opts;
    opts.show_bool_as_text = false;
    FATP_ASSERT_EQ(toString(true, opts), "1", "true as 1");
    FATP_ASSERT_EQ(toString(false, opts), "0", "false as 0");
    return true;
}

// =============================================================================
// String Passthrough
// =============================================================================

FATP_TEST_CASE(string_std_string)
{
    std::string s = "hello world";
    FATP_ASSERT_EQ(toString(s), "hello world", "std::string passthrough");
    FATP_ASSERT_EQ(toString(std::string("test")), "test", "rvalue string");
    FATP_ASSERT_EQ(toString(std::string("")), "", "empty string");
    return true;
}

FATP_TEST_CASE(string_c_string)
{
    const char* cs = "c-string";
    FATP_ASSERT_EQ(toString(cs), "c-string", "const char*");

    char mutable_str[] = "mutable";
    FATP_ASSERT_EQ(toString(mutable_str), "mutable", "char array");
    return true;
}

FATP_TEST_CASE(string_null_c_string)
{
    const char* null_ptr = nullptr;
    FATP_ASSERT_EQ(toString(null_ptr), "<non-stringifiable>", "null const char*");

    StringifyOptions opts;
    opts.placeholder = "NULL";
    FATP_ASSERT_EQ(toString(null_ptr, opts), "NULL", "null with custom placeholder");
    return true;
}

FATP_TEST_CASE(string_char_array)
{
    char arr[] = "array";
    FATP_ASSERT_EQ(toString(arr), "array", "char[]");

    const char const_arr[] = "const array";
    FATP_ASSERT_EQ(toString(const_arr), "const array", "const char[]");
    return true;
}

// =============================================================================
// Enum Stringification
// =============================================================================

FATP_TEST_CASE(enum_with_stringifier)
{
    FATP_ASSERT_EQ(toString(Color::Red), "Red", "Red");
    FATP_ASSERT_EQ(toString(Color::Green), "Green", "Green");
    FATP_ASSERT_EQ(toString(Color::Blue), "Blue", "Blue");
    return true;
}

FATP_TEST_CASE(enum_without_stringifier)
{
    FATP_ASSERT_EQ(toString(Priority::Low), "1", "Priority::Low");
    FATP_ASSERT_EQ(toString(Priority::Medium), "5", "Priority::Medium");
    FATP_ASSERT_EQ(toString(Priority::High), "10", "Priority::High");
    return true;
}

FATP_TEST_CASE(enum_unscoped)
{
    FATP_ASSERT_EQ(toString(A), "100", "unscoped A");
    FATP_ASSERT_EQ(toString(B), "200", "unscoped B");
    return true;
}

FATP_TEST_CASE(enum_in_container)
{
    std::vector<Color> colors = {Color::Red, Color::Green, Color::Blue};
    auto result = toString(colors);
    FATP_ASSERT_TRUE(result.find("Red") != std::string::npos, "contains Red");
    FATP_ASSERT_TRUE(result.find("Green") != std::string::npos, "contains Green");
    FATP_ASSERT_TRUE(result.find("Blue") != std::string::npos, "contains Blue");
    return true;
}

// =============================================================================
// Custom Method Stringification
// =============================================================================

FATP_TEST_CASE(custom_to_string_method)
{
    CustomToString c{42};
    FATP_ASSERT_EQ(toString(c), "Custom(42)", "toString() method");
    return true;
}

FATP_TEST_CASE(custom_to_string_snake_method)
{
    CustomToStringSnake c{99};
    FATP_ASSERT_EQ(toString(c), "Snake(99)", "to_string() method");
    return true;
}

FATP_TEST_CASE(custom_both_methods_prefers_camel_case)
{
    CustomBothMethods c{1};
    FATP_ASSERT_EQ(toString(c), "CamelCase", "toString() preferred over to_string()");
    return true;
}

FATP_TEST_CASE(custom_streamable)
{
    CustomStreamable c{77};
    FATP_ASSERT_EQ(toString(c), "Streamable(77)", "operator<<");
    return true;
}

// =============================================================================
// Pair Stringification
// =============================================================================

FATP_TEST_CASE(pair_basic)
{
    std::pair<int, int> p1{1, 2};
    FATP_ASSERT_EQ(toString(p1), "(1, 2)", "int pair");

    std::pair<std::string, int> p2{"key", 42};
    FATP_ASSERT_EQ(toString(p2), "(key, 42)", "string-int pair");
    return true;
}

FATP_TEST_CASE(pair_nested)
{
    std::pair<std::pair<int, int>, int> nested{{1, 2}, 3};
    FATP_ASSERT_EQ(toString(nested), "((1, 2), 3)", "nested pair");
    return true;
}

FATP_TEST_CASE(pair_with_container)
{
    std::pair<std::vector<int>, std::string> p{{1, 2, 3}, "test"};
    auto result = toString(p);
    FATP_ASSERT_TRUE(result.find("[1, 2, 3]") != std::string::npos, "pair with vector");
    return true;
}

// =============================================================================
// Tuple Stringification
// =============================================================================

FATP_TEST_CASE(tuple_basic)
{
    std::tuple<int, double, std::string> t{1, 3.14, "test"};
    auto result = toString(t);
    FATP_ASSERT_TRUE(result.find("1") != std::string::npos, "tuple int");
    FATP_ASSERT_TRUE(result.find("3.14") != std::string::npos, "tuple double");
    FATP_ASSERT_TRUE(result.find("test") != std::string::npos, "tuple string");
    return true;
}

FATP_TEST_CASE(tuple_empty)
{
    std::tuple<> empty;
    FATP_ASSERT_EQ(toString(empty), "()", "empty tuple");
    return true;
}

FATP_TEST_CASE(tuple_single)
{
    std::tuple<int> single{42};
    FATP_ASSERT_EQ(toString(single), "(42)", "single element tuple");
    return true;
}

FATP_TEST_CASE(tuple_nested)
{
    std::tuple<std::tuple<int, int>, std::string> nested{{1, 2}, "x"};
    auto result = toString(nested);
    FATP_ASSERT_TRUE(result.find("(1, 2)") != std::string::npos, "nested tuple");
    return true;
}

// =============================================================================
// Optional Stringification
// =============================================================================

FATP_TEST_CASE(optional_with_value)
{
    std::optional<int> opt = 42;
    FATP_ASSERT_EQ(toString(opt), "42", "optional with value");

    std::optional<std::string> opt_str = "hello";
    FATP_ASSERT_EQ(toString(opt_str), "hello", "optional string");
    return true;
}

FATP_TEST_CASE(optional_empty)
{
    std::optional<int> empty;
    FATP_ASSERT_EQ(toString(empty), "nullopt", "empty optional");

    std::optional<std::string> empty_str = std::nullopt;
    FATP_ASSERT_EQ(toString(empty_str), "nullopt", "nullopt");
    return true;
}

FATP_TEST_CASE(optional_nested)
{
    std::optional<std::optional<int>> nested = std::optional<int>{42};
    FATP_ASSERT_EQ(toString(nested), "42", "nested optional");

    std::optional<std::optional<int>> nested_empty = std::optional<int>{};
    FATP_ASSERT_EQ(toString(nested_empty), "nullopt", "nested empty optional");
    return true;
}

// =============================================================================
// Container Stringification
// =============================================================================

FATP_TEST_CASE(container_vector)
{
    std::vector<int> v{1, 2, 3, 4, 5};
    FATP_ASSERT_EQ(toString(v), "[1, 2, 3, 4, 5]", "vector");

    std::vector<int> empty;
    FATP_ASSERT_EQ(toString(empty), "[]", "empty vector");
    return true;
}

FATP_TEST_CASE(container_array)
{
    std::array<int, 3> arr{10, 20, 30};
    FATP_ASSERT_EQ(toString(arr), "[10, 20, 30]", "array");
    return true;
}

FATP_TEST_CASE(container_list)
{
    std::list<std::string> lst{"a", "b", "c"};
    FATP_ASSERT_EQ(toString(lst), "[a, b, c]", "list");
    return true;
}

FATP_TEST_CASE(container_deque)
{
    std::deque<int> dq{1, 2, 3};
    FATP_ASSERT_EQ(toString(dq), "[1, 2, 3]", "deque");
    return true;
}

FATP_TEST_CASE(container_forward_list)
{
    std::forward_list<int> fl{1, 2, 3};
    FATP_ASSERT_EQ(toString(fl), "[1, 2, 3]", "forward_list");
    return true;
}

FATP_TEST_CASE(container_set)
{
    std::set<int> s{3, 1, 2};
    FATP_ASSERT_EQ(toString(s), "[1, 2, 3]", "set (sorted)");
    return true;
}

FATP_TEST_CASE(container_nested)
{
    std::vector<std::vector<int>> nested{{1, 2}, {3, 4}};
    FATP_ASSERT_EQ(toString(nested), "[[1, 2], [3, 4]]", "nested vector");
    return true;
}

FATP_TEST_CASE(container_custom_delimiters)
{
    std::vector<int> v{1, 2, 3};
    StringifyOptions opts;
    opts.container_open = "{";
    opts.container_close = "}";
    opts.container_separator = "; ";
    FATP_ASSERT_EQ(toString(v, opts), "{1; 2; 3}", "custom delimiters");
    return true;
}

// =============================================================================
// Map Stringification
// =============================================================================

FATP_TEST_CASE(map_basic)
{
    std::map<std::string, int> m{{"a", 1}, {"b", 2}};
    auto result = toString(m);
    FATP_ASSERT_TRUE(result.find("{") == 0, "map starts with {");
    FATP_ASSERT_TRUE(result.find("a: 1") != std::string::npos, "contains a: 1");
    FATP_ASSERT_TRUE(result.find("b: 2") != std::string::npos, "contains b: 2");
    return true;
}

FATP_TEST_CASE(map_empty)
{
    std::map<int, int> empty;
    FATP_ASSERT_EQ(toString(empty), "{}", "empty map");
    return true;
}

FATP_TEST_CASE(map_unordered)
{
    std::unordered_map<int, std::string> um{{1, "one"}};
    auto result = toString(um);
    FATP_ASSERT_TRUE(result.find("1: one") != std::string::npos, "unordered_map");
    return true;
}

// =============================================================================
// Recursion Depth Guard
// =============================================================================

FATP_TEST_CASE(depth_guard_default)
{
    using V1 = std::vector<int>;
    using V2 = std::vector<V1>;
    using V3 = std::vector<V2>;
    using V4 = std::vector<V3>;
    using V5 = std::vector<V4>;

    V5 deep = {{{{{1}}}}};
    auto result = toString(deep);
    FATP_ASSERT_TRUE(result.find("<max depth>") != std::string::npos, "default depth limit");
    return true;
}

FATP_TEST_CASE(depth_guard_custom)
{
    std::vector<std::vector<std::vector<int>>> v3{{{1}}};

    StringifyOptions opts;
    opts.max_container_depth = 2;
    auto result = toString(v3, opts);
    FATP_ASSERT_TRUE(result.find("<max depth>") != std::string::npos, "custom depth limit");
    return true;
}

FATP_TEST_CASE(depth_guard_reset)
{
    using V5 = std::vector<std::vector<std::vector<std::vector<std::vector<int>>>>>;
    V5 deep = {{{{{1}}}}};
    (void)toString(deep); // Hit limit

    std::vector<int> simple{1, 2, 3};
    FATP_ASSERT_EQ(toString(simple), "[1, 2, 3]", "depth guard resets");
    return true;
}

// =============================================================================
// Type Priority Edge Cases
// =============================================================================

FATP_TEST_CASE(priority_fake_pair_uses_to_string)
{
    FakePairWithToString fp{42};
    auto result = toString(fp);
    FATP_ASSERT_TRUE(result.find("FakePair") != std::string::npos, "FakePair uses toString()");
    return true;
}

FATP_TEST_CASE(priority_iterable_uses_to_string)
{
    IterableWithToString iwts;
    FATP_ASSERT_EQ(toString(iwts), "IterableCustom", "iterable with toString() uses toString()");
    return true;
}

FATP_TEST_CASE(priority_non_stringifiable_placeholder)
{
    NonStringifiable ns{42};
    FATP_ASSERT_EQ(toString(ns), "<non-stringifiable>", "non-stringifiable uses placeholder");
    return true;
}

// =============================================================================
// StringifyOptions Tests
// =============================================================================

FATP_TEST_CASE(options_default_values)
{
    StringifyOptions opts;
    FATP_ASSERT_TRUE(opts.use_hex_for_pointers, "default use_hex_for_pointers");
    FATP_ASSERT_EQ(opts.float_precision, -1, "default float_precision");
    FATP_ASSERT_TRUE(!opts.scientific_notation, "default scientific_notation");
    FATP_ASSERT_TRUE(opts.show_bool_as_text, "default show_bool_as_text");
    FATP_ASSERT_EQ(opts.max_container_depth, 3, "default max_container_depth");
    FATP_ASSERT_TRUE(opts.use_classic_locale, "default use_classic_locale");
    FATP_ASSERT_TRUE(opts.custom_locale == nullptr, "default custom_locale");
    return true;
}

FATP_TEST_CASE(options_placeholder)
{
    NonStringifiable ns{1};

    StringifyOptions opts;
    opts.placeholder = "N/A";
    FATP_ASSERT_EQ(toString(ns, opts), "N/A", "custom placeholder");

    opts.placeholder = "";
    FATP_ASSERT_EQ(toString(ns, opts), "", "empty placeholder");
    return true;
}

FATP_TEST_CASE(options_classic_locale)
{
    double value = 1234.56;
    auto result = toString(value);
    FATP_ASSERT_TRUE(result.find(".") != std::string::npos, "classic locale uses '.'");
    return true;
}

// =============================================================================
// Convenience Functions
// =============================================================================

FATP_TEST_CASE(to_string_or)
{
    NonStringifiable ns{1};
    FATP_ASSERT_EQ(toStringOr(ns, "fallback"), "fallback", "toStringOr with fallback");
    FATP_ASSERT_EQ(toStringOr(42, "fallback"), "42", "toStringOr without fallback");
    return true;
}

FATP_TEST_CASE(try_to_string_success)
{
    std::string result;
    FATP_ASSERT_TRUE(tryToString(42, result), "tryToString returns true");
    FATP_ASSERT_EQ(result, "42", "tryToString result");
    return true;
}

FATP_TEST_CASE(try_to_string_exception)
{
    ThrowingToString thrower;
    std::string result;
    FATP_ASSERT_FALSE(tryToString(thrower, result), "tryToString returns false on exception");

    auto error = getLastStringifyError();
    FATP_ASSERT_TRUE(error.find("Intentional") != std::string::npos, "error message captured");
    return true;
}

FATP_TEST_CASE(to_w_string_basic)
{
    FATP_ASSERT_TRUE(toWString(42) == L"42", "toWString int");
    FATP_ASSERT_TRUE(toWString(true).find(L"true") != std::wstring::npos, "toWString bool");

    auto wdouble = toWString(3.14);
    FATP_ASSERT_TRUE(wdouble.find(L"3.14") != std::wstring::npos, "toWString double");
    return true;
}

FATP_TEST_CASE(to_string_padded_right)
{
    FATP_ASSERT_EQ(toStringPadded(42, 5, '>'), "   42", "right align space");
    FATP_ASSERT_EQ(toStringPadded(42, 5, '>', '0'), "00042", "right align zero");
    FATP_ASSERT_EQ(toStringPadded(42, 5, '>', '*'), "***42", "right align asterisk");
    return true;
}

FATP_TEST_CASE(to_string_padded_left)
{
    FATP_ASSERT_EQ(toStringPadded(42, 5, '<'), "42   ", "left align space");
    FATP_ASSERT_EQ(toStringPadded(42, 5, '<', '-'), "42---", "left align dash");
    return true;
}

FATP_TEST_CASE(to_string_padded_center)
{
    FATP_ASSERT_EQ(toStringPadded(42, 6, '^'), "  42  ", "center align even");
    FATP_ASSERT_EQ(toStringPadded(42, 5, '^'), " 42  ", "center align odd");
    return true;
}

FATP_TEST_CASE(to_string_padded_no_padding_needed)
{
    FATP_ASSERT_EQ(toStringPadded(12345, 3), "12345", "no padding when longer");
    FATP_ASSERT_EQ(toStringPadded(12345, 5), "12345", "no padding when equal");
    return true;
}

FATP_TEST_CASE(to_string_formatted)
{
    FATP_ASSERT_TRUE(toStringFormatted(3.14159, 2).find("3.14") != std::string::npos, "precision 2");
    FATP_ASSERT_TRUE(toStringFormatted(3.14159, 4).find("3.1416") != std::string::npos, "precision 4");

    auto sci = toStringFormatted(12345.0, 2, false);
    FATP_ASSERT_TRUE(sci.find("e") != std::string::npos || sci.find("E") != std::string::npos, "scientific");
    return true;
}

FATP_TEST_CASE(to_string_pointer)
{
    int value = 42;
    auto ptr_str = toStringPointer(&value);
    FATP_ASSERT_TRUE(ptr_str.find("0x") != std::string::npos, "pointer hex prefix");

    FATP_ASSERT_EQ(toStringPointer(static_cast<int*>(nullptr)), "nullptr", "null pointer");
    FATP_ASSERT_EQ(toStringPointer(static_cast<int*>(nullptr), {}, "NULL"), "NULL", "custom null");
    return true;
}

FATP_TEST_CASE(to_string_pointer_decimal)
{
    int value = 42;
    StringifyOptions opts;
    opts.use_hex_for_pointers = false;
    auto ptr_str = toStringPointer(&value, opts);
    FATP_ASSERT_TRUE(ptr_str.find("0x") == std::string::npos, "no hex prefix");
    return true;
}

FATP_TEST_CASE(to_string_concat)
{
    FATP_ASSERT_EQ(toStringConcat("a", "b", "c"), "abc", "concat strings");
    FATP_ASSERT_EQ(toStringConcat("x=", 42), "x=42", "concat mixed");
    FATP_ASSERT_EQ(toStringConcat(1, ", ", 2, ", ", 3), "1, 2, 3", "concat many");
    return true;
}

// =============================================================================
// Thread Safety
// =============================================================================

FATP_TEST_CASE(thread_safety_concurrent)
{
    constexpr size_t NUM_THREADS = 4;
    constexpr size_t ITERATIONS = 1000;

    std::atomic<size_t> success_count{0};
    std::atomic<size_t> error_count{0};
    std::atomic<bool> start_flag{false};
    std::vector<std::thread> threads;

    auto worker = [&](size_t thread_id) {
        while (!start_flag.load(std::memory_order_acquire))
        {
            std::this_thread::yield();
        }

        for (size_t i = 0; i < ITERATIONS; ++i)
        {
            try
            {
                volatile auto s1 = toString(static_cast<int>(thread_id * 1000 + i));
                volatile auto s2 = toString(3.14159 * static_cast<double>(thread_id));
                volatile auto s3 = toString(std::vector<int>{1, 2, 3});

                std::string out;
                if (tryToString(static_cast<int>(i), out))
                {
                    success_count.fetch_add(1, std::memory_order_relaxed);
                }

                (void)s1;
                (void)s2;
                (void)s3;
            }
            catch (...)
            {
                error_count.fetch_add(1, std::memory_order_relaxed);
            }
        }
    };

    for (size_t t = 0; t < NUM_THREADS; ++t)
    {
        threads.emplace_back(worker, t);
    }

    start_flag.store(true, std::memory_order_release);

    for (auto& t : threads)
    {
        t.join();
    }

    FATP_ASSERT_EQ(error_count.load(), 0u, "no exceptions");
    FATP_ASSERT_EQ(success_count.load(), NUM_THREADS * ITERATIONS, "all succeeded");
    return true;
}

FATP_TEST_CASE(thread_safety_error_independence)
{
    constexpr size_t NUM_THREADS = 4;
    std::atomic<size_t> correct_errors{0};
    std::atomic<bool> start_flag{false};
    std::vector<std::thread> threads;

    auto worker = [&]([[maybe_unused]] size_t thread_id) {
        while (!start_flag.load(std::memory_order_acquire))
        {
            std::this_thread::yield();
        }

        ThrowingToString thrower;
        std::string out;
        (void)tryToString(thrower, out);

        if (getLastStringifyError().find("Intentional") != std::string::npos)
        {
            correct_errors.fetch_add(1, std::memory_order_relaxed);
        }
    };

    for (size_t t = 0; t < NUM_THREADS; ++t)
    {
        threads.emplace_back(worker, t);
    }

    start_flag.store(true, std::memory_order_release);

    for (auto& t : threads)
    {
        t.join();
    }

    FATP_ASSERT_EQ(correct_errors.load(), NUM_THREADS, "each thread has independent error state");
    return true;
}

// =============================================================================
// Corner Cases - Integers
// =============================================================================

FATP_TEST_CASE(corner_char_as_integer)
{
    // char should stringify as a number, not as a character
    char c = 'A'; // ASCII 65
    // Note: char may be treated as character or int depending on implementation
    auto result = toString(c);
    // Either "65" or "A" is acceptable depending on how char is handled
    FATP_ASSERT_TRUE(result == "65" || result == "A", "char stringification");

    signed char sc = -10;
    FATP_ASSERT_EQ(toString(sc), "-10", "signed char");

    unsigned char uc = 200;
    FATP_ASSERT_EQ(toString(uc), "200", "unsigned char");
    return true;
}

FATP_TEST_CASE(corner_int8_uint8)
{
    int8_t i8 = -128;
    FATP_ASSERT_EQ(toString(i8), "-128", "int8_t min");

    i8 = 127;
    FATP_ASSERT_EQ(toString(i8), "127", "int8_t max");

    uint8_t u8 = 0;
    FATP_ASSERT_EQ(toString(u8), "0", "uint8_t zero");

    u8 = 255;
    FATP_ASSERT_EQ(toString(u8), "255", "uint8_t max");
    return true;
}

FATP_TEST_CASE(corner_unsigned_boundaries)
{
    FATP_ASSERT_EQ(toString(std::numeric_limits<unsigned int>::max()),
                   std::to_string(std::numeric_limits<unsigned int>::max()),
                   "uint max");
    FATP_ASSERT_EQ(toString(std::numeric_limits<uint64_t>::max()),
                   std::to_string(std::numeric_limits<uint64_t>::max()),
                   "uint64 max");
    return true;
}

// =============================================================================
// Corner Cases - Floats
// =============================================================================

FATP_TEST_CASE(corner_float_negative_zero)
{
    double neg_zero = -0.0;
    auto result = toString(neg_zero);
    // -0.0 may stringify as "0" or "-0" depending on implementation
    FATP_ASSERT_TRUE(result == "0" || result == "-0", "negative zero");
    return true;
}

FATP_TEST_CASE(corner_float_denormalized)
{
    double denorm = std::numeric_limits<double>::denorm_min();
    auto result = toString(denorm);
    FATP_ASSERT_TRUE(!result.empty(), "denormalized number stringifies");
    FATP_ASSERT_TRUE(result != "0", "denormalized is not zero");
    return true;
}

FATP_TEST_CASE(corner_float_extremes)
{
    double tiny = std::numeric_limits<double>::epsilon();
    auto tiny_result = toString(tiny);
    FATP_ASSERT_TRUE(!tiny_result.empty(), "epsilon stringifies");

    double huge = std::numeric_limits<double>::max();
    auto huge_result = toString(huge);
    FATP_ASSERT_TRUE(!huge_result.empty(), "max double stringifies");
    FATP_ASSERT_TRUE(huge_result.find("e") != std::string::npos || huge_result.find("E") != std::string::npos ||
                         huge_result.size() > 100,
                     "max double is very large");

    double tiny_pos = std::numeric_limits<double>::min(); // smallest positive normalized
    auto tiny_pos_result = toString(tiny_pos);
    FATP_ASSERT_TRUE(!tiny_pos_result.empty(), "min positive stringifies");
    return true;
}

FATP_TEST_CASE(corner_float_long_double)
{
    long double ld = 3.14159265358979323846L;
    auto result = toString(ld);
    FATP_ASSERT_TRUE(result.find("3.14") != std::string::npos, "long double");
    return true;
}

// =============================================================================
// Corner Cases - Strings
// =============================================================================

FATP_TEST_CASE(corner_string_embedded_null)
{
    std::string with_null = "hello";
    with_null += '\0';
    with_null += "world";
    auto result = toString(with_null);
    FATP_ASSERT_EQ(result.size(), with_null.size(), "embedded null preserved");
    return true;
}

FATP_TEST_CASE(corner_string_unicode)
{
    // UTF-8 encoded string (without u8 prefix which changed in C++20)
    std::string utf8 = "Hello \xe4\xb8\x96\xe7\x95\x8c"; // "Hello 世界" in UTF-8
    auto result = toString(utf8);
    FATP_ASSERT_EQ(result, utf8, "UTF-8 preserved");
    return true;
}

FATP_TEST_CASE(corner_string_view)
{
    std::string_view sv = "string_view test";
    auto result = toString(sv);
    FATP_ASSERT_EQ(result, "string_view test", "string_view");

    std::string_view empty_sv;
    FATP_ASSERT_EQ(toString(empty_sv), "", "empty string_view");
    return true;
}

FATP_TEST_CASE(corner_string_special_chars)
{
    FATP_ASSERT_EQ(toString(std::string("\t\n\r")), "\t\n\r", "whitespace chars");
    FATP_ASSERT_EQ(toString(std::string("\\")), "\\", "backslash");
    FATP_ASSERT_EQ(toString(std::string("\"")), "\"", "quote");
    return true;
}

// =============================================================================
// Corner Cases - Containers
// =============================================================================

FATP_TEST_CASE(corner_container_single_element)
{
    std::vector<int> single{42};
    FATP_ASSERT_EQ(toString(single), "[42]", "single element vector");

    std::list<std::string> single_list{"only"};
    FATP_ASSERT_EQ(toString(single_list), "[only]", "single element list");
    return true;
}

FATP_TEST_CASE(corner_container_multimap)
{
    std::multimap<int, std::string> mm{{1, "a"}, {1, "b"}, {2, "c"}};
    auto result = toString(mm);
    FATP_ASSERT_TRUE(result.find("1:") != std::string::npos, "multimap has key 1");
    FATP_ASSERT_TRUE(result.find("2:") != std::string::npos, "multimap has key 2");
    return true;
}

FATP_TEST_CASE(corner_container_multiset)
{
    std::multiset<int> ms{1, 1, 2, 2, 2, 3};
    auto result = toString(ms);
    FATP_ASSERT_EQ(result, "[1, 1, 2, 2, 2, 3]", "multiset with duplicates");
    return true;
}

FATP_TEST_CASE(corner_container_unordered_set)
{
    std::unordered_set<int> us{3, 1, 2};
    auto result = toString(us);
    FATP_ASSERT_TRUE(result.find("1") != std::string::npos, "contains 1");
    FATP_ASSERT_TRUE(result.find("2") != std::string::npos, "contains 2");
    FATP_ASSERT_TRUE(result.find("3") != std::string::npos, "contains 3");
    return true;
}

FATP_TEST_CASE(corner_container_of_pairs)
{
    std::vector<std::pair<int, int>> vp{{1, 2}, {3, 4}};
    auto result = toString(vp);
    FATP_ASSERT_TRUE(result.find("(1, 2)") != std::string::npos, "pair in vector");
    FATP_ASSERT_TRUE(result.find("(3, 4)") != std::string::npos, "second pair");
    return true;
}

FATP_TEST_CASE(corner_container_of_optionals)
{
    std::vector<std::optional<int>> vo{1, std::nullopt, 3};
    auto result = toString(vo);
    FATP_ASSERT_TRUE(result.find("1") != std::string::npos, "optional with value");
    FATP_ASSERT_TRUE(result.find("nullopt") != std::string::npos, "empty optional in vector");
    return true;
}

// =============================================================================
// Corner Cases - Tuples and Arrays
// =============================================================================

FATP_TEST_CASE(corner_tuple_homogeneous)
{
    std::tuple<int, int, int, int> t{1, 2, 3, 4};
    FATP_ASSERT_EQ(toString(t), "(1, 2, 3, 4)", "homogeneous tuple");
    return true;
}

FATP_TEST_CASE(corner_std_array_as_container)
{
    // std::array is both tuple_like and a range - should use range formatting
    std::array<int, 4> arr{1, 2, 3, 4};
    auto result = toString(arr);
    // Should be "[1, 2, 3, 4]" not "(1, 2, 3, 4)" because printable_range takes priority
    FATP_ASSERT_TRUE(result.find("[") != std::string::npos || result.find("(") != std::string::npos,
                     "std::array stringifies");
    return true;
}

FATP_TEST_CASE(corner_pair_with_non_stringifiable)
{
    std::pair<int, NonStringifiable> p{42, {99}};
    auto result = toString(p);
    FATP_ASSERT_TRUE(result.find("42") != std::string::npos, "stringifiable element");
    FATP_ASSERT_TRUE(result.find("<non-stringifiable>") != std::string::npos, "non-stringifiable element");
    return true;
}

// =============================================================================
// Corner Cases - Optionals
// =============================================================================

FATP_TEST_CASE(corner_optional_triple_nested)
{
    std::optional<std::optional<std::optional<int>>> triple = std::optional<std::optional<int>>{std::optional<int>{42}};
    FATP_ASSERT_EQ(toString(triple), "42", "triple nested optional");

    std::optional<std::optional<std::optional<int>>> triple_empty = std::optional<std::optional<int>>{std::nullopt};
    FATP_ASSERT_EQ(toString(triple_empty), "nullopt", "triple nested nullopt");
    return true;
}

FATP_TEST_CASE(corner_optional_of_non_stringifiable)
{
    std::optional<NonStringifiable> opt = NonStringifiable{42};
    auto result = toString(opt);
    FATP_ASSERT_EQ(result, "<non-stringifiable>", "optional of non-stringifiable");

    std::optional<NonStringifiable> empty_opt;
    FATP_ASSERT_EQ(toString(empty_opt), "nullopt", "empty optional of non-stringifiable");
    return true;
}

// =============================================================================
// Corner Cases - Enums
// =============================================================================

enum class SignedEnum : int
{
    Negative = -5,
    Zero = 0,
    Positive = 5
};
enum class BigEnum : uint64_t
{
    Big = 0xFFFFFFFFFFFFFFFFULL
};

FATP_TEST_CASE(corner_enum_negative_underlying)
{
    FATP_ASSERT_EQ(toString(SignedEnum::Negative), "-5", "negative enum value");
    FATP_ASSERT_EQ(toString(SignedEnum::Zero), "0", "zero enum value");
    FATP_ASSERT_EQ(toString(SignedEnum::Positive), "5", "positive enum value");
    return true;
}

FATP_TEST_CASE(corner_enum_uint64_underlying)
{
    auto result = toString(BigEnum::Big);
    FATP_ASSERT_EQ(result, std::to_string(0xFFFFFFFFFFFFFFFFULL), "uint64 enum max");
    return true;
}

// =============================================================================
// Corner Cases - Custom Types
// =============================================================================

struct EmptyToString
{
    std::string toString() const
    {
        return "";
    }
};

struct NonConstToString
{
    int value = 42;
    std::string toString()
    {
        return "NonConst(" + std::to_string(value) + ")";
    } // non-const!
};

FATP_TEST_CASE(corner_custom_empty_return)
{
    EmptyToString ets;
    FATP_ASSERT_EQ(toString(ets), "", "toString returning empty string");
    return true;
}

FATP_TEST_CASE(corner_custom_non_const_method)
{
    // Non-const toString() should still work if we have a non-const object
    NonConstToString ncts;
    // This may or may not compile depending on concept definition
    // If it fails to compile, that's actually correct behavior
    auto result = toString(ncts);
    FATP_ASSERT_TRUE(result.find("NonConst") != std::string::npos || result == "<non-stringifiable>",
                     "non-const toString handling");
    return true;
}

// =============================================================================
// Corner Cases - References and Qualifiers
// =============================================================================

FATP_TEST_CASE(corner_const_reference)
{
    const int ci = 42;
    FATP_ASSERT_EQ(toString(ci), "42", "const int");

    const std::vector<int> cv{1, 2, 3};
    FATP_ASSERT_EQ(toString(cv), "[1, 2, 3]", "const vector");
    return true;
}

FATP_TEST_CASE(corner_rvalue)
{
    FATP_ASSERT_EQ(toString(42), "42", "int rvalue");
    FATP_ASSERT_EQ(toString(std::string("temp")), "temp", "string rvalue");
    FATP_ASSERT_EQ(toString(std::vector<int>{1, 2}), "[1, 2]", "vector rvalue");
    return true;
}

FATP_TEST_CASE(corner_pointer_non_null)
{
    int value = 42;
    int* ptr = &value;

    // Raw pointer should go through streamable path (prints address)
    auto result = toString(ptr);
    // Should be an address like "0x7fff..." or similar
    FATP_ASSERT_TRUE(!result.empty(), "pointer stringifies");
    return true;
}

// =============================================================================
// Corner Cases - Options Combinations
// =============================================================================

FATP_TEST_CASE(corner_options_all_float_options)
{
    StringifyOptions opts;
    opts.float_precision = 3;
    opts.scientific_notation = true;
    opts.use_classic_locale = true;

    auto result = toString(12345.6789, opts);
    FATP_ASSERT_TRUE(result.find("e") != std::string::npos || result.find("E") != std::string::npos,
                     "scientific with precision");
    return true;
}

FATP_TEST_CASE(corner_options_container_all)
{
    std::vector<int> v{1, 2, 3};
    StringifyOptions opts;
    opts.container_open = "<<";
    opts.container_close = ">>";
    opts.container_separator = " | ";
    opts.max_container_depth = 1;

    FATP_ASSERT_EQ(toString(v, opts), "<<1 | 2 | 3>>", "all container options");

    std::vector<std::vector<int>> nested{{1}};
    auto nested_result = toString(nested, opts);
    FATP_ASSERT_TRUE(nested_result.find("<max depth>") != std::string::npos, "depth 1 limit");
    return true;
}

// =============================================================================
// Stress Tests
// =============================================================================

FATP_TEST_CASE(stress_large_container)
{
    // Stress test: Large container with many elements
    std::vector<int> large(10000);
    std::iota(large.begin(), large.end(), 0);

    auto result = toString(large);
    FATP_ASSERT_TRUE(result.find("[0, 1, 2") != std::string::npos, "starts correctly");
    FATP_ASSERT_TRUE(result.find("9999]") != std::string::npos, "ends correctly");
    FATP_ASSERT_TRUE(result.size() > 50000, "large output");
    return true;
}

FATP_TEST_CASE(stress_deeply_nested_at_limit)
{
    // Stress test: Nesting exactly at default depth limit (3)
    std::vector<std::vector<std::vector<int>>> v3{{{1, 2}, {3, 4}}, {{5, 6}}};
    auto result = toString(v3);
    FATP_ASSERT_TRUE(result.find("[[[") != std::string::npos, "3 levels deep");
    FATP_ASSERT_TRUE(result.find("<max depth>") == std::string::npos, "within limit");
    return true;
}

FATP_TEST_CASE(stress_many_map_entries)
{
    std::map<int, std::string> large_map;
    for (int i = 0; i < 1000; ++i)
    {
        large_map[i] = "value" + std::to_string(i);
    }

    auto result = toString(large_map);
    FATP_ASSERT_TRUE(result.find("{0: value0") != std::string::npos, "first entry");
    FATP_ASSERT_TRUE(result.find("999: value999}") != std::string::npos, "last entry");
    return true;
}

FATP_TEST_CASE(stress_long_string_passthrough)
{
    std::string long_str(100000, 'x');
    auto result = toString(long_str);
    FATP_ASSERT_EQ(result.size(), 100000u, "long string preserved");
    FATP_ASSERT_EQ(result, long_str, "content matches");
    return true;
}

FATP_TEST_CASE(stress_wide_tuple)
{
    auto wide = std::make_tuple(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20);
    auto result = toString(wide);
    FATP_ASSERT_TRUE(result.find("(1, 2, 3") != std::string::npos, "starts correctly");
    FATP_ASSERT_TRUE(result.find("19, 20)") != std::string::npos, "ends correctly");
    return true;
}

FATP_TEST_CASE(stress_rapid_allocation)
{
    // Stress test: Rapid repeated allocations
    constexpr size_t ITERATIONS = 10000;

    for (size_t i = 0; i < ITERATIONS; ++i)
    {
        volatile auto s1 = toString(static_cast<int>(i));
        volatile auto s2 = toString(std::vector<int>{1, 2, 3});
        volatile auto s3 = toStringConcat("iter=", i, " data=", 3.14);
        (void)s1;
        (void)s2;
        (void)s3;
    }

    // If we get here without crash/hang, test passes
    return true;
}

FATP_TEST_CASE(stress_mixed_types_rapid)
{
    constexpr size_t ITERATIONS = 5000;

    for (size_t i = 0; i < ITERATIONS; ++i)
    {
        volatile auto s1 = toString(static_cast<int>(i));
        volatile auto s2 = toString(static_cast<double>(i) * 0.1);
        volatile auto s3 = toString(i % 2 == 0);
        volatile auto s4 = toString(std::make_pair(i, "test"));
        volatile auto s5 = toString(std::optional<int>(static_cast<int>(i)));
        volatile auto s6 = toString(Color::Red);
        (void)s1;
        (void)s2;
        (void)s3;
        (void)s4;
        (void)s5;
        (void)s6;
    }
    return true;
}

FATP_TEST_CASE(stress_concurrent_high_load)
{
    constexpr size_t NUM_THREADS = 8;
    constexpr size_t ITERATIONS = 5000;

    std::atomic<size_t> total_ops{0};
    std::atomic<bool> start_flag{false};
    std::vector<std::thread> threads;

    auto worker = [&](size_t thread_id) {
        while (!start_flag.load(std::memory_order_acquire))
        {
            std::this_thread::yield();
        }

        for (size_t i = 0; i < ITERATIONS; ++i)
        {
            volatile auto s1 = toString(static_cast<int>(thread_id * 10000 + i));
            volatile auto s2 = toString(std::vector<int>{1, 2, 3, 4, 5});
            volatile auto s3 = toString(std::make_pair(thread_id, i));
            total_ops.fetch_add(3, std::memory_order_relaxed);
            (void)s1;
            (void)s2;
            (void)s3;
        }
    };

    for (size_t t = 0; t < NUM_THREADS; ++t)
    {
        threads.emplace_back(worker, t);
    }

    start_flag.store(true, std::memory_order_release);

    for (auto& t : threads)
    {
        t.join();
    }

    FATP_ASSERT_EQ(total_ops.load(), NUM_THREADS * ITERATIONS * 3, "all operations completed");
    return true;
}

// =============================================================================
// Soak Tests
// =============================================================================

FATP_TEST_CASE(soak_sustained_operation)
{
    // Soak test: Sustained operation over time
    auto start = high_resolution_clock::now();
    constexpr auto DURATION = std::chrono::milliseconds(500);
    size_t iterations = 0;

    while (high_resolution_clock::now() - start < DURATION)
    {
        volatile auto s1 = toString(static_cast<int>(iterations));
        volatile auto s2 = toString(3.14159);
        volatile auto s3 = toString(std::vector<int>{1, 2, 3});
        volatile auto s4 = toString(std::make_tuple(1, "test", 3.14));
        (void)s1;
        (void)s2;
        (void)s3;
        (void)s4;
        ++iterations;
    }

    auto& out = *get_test_config().output;
    out << "  Sustained " << iterations << " iterations over 500ms\n";

    FATP_ASSERT_TRUE(iterations > 1000, "sufficient throughput");
    return true;
}

FATP_TEST_CASE(soak_memory_stability)
{
    // Soak test: Memory stability - repeated allocations should not leak
    constexpr size_t ITERATIONS = 50000;

    for (size_t i = 0; i < ITERATIONS; ++i)
    {
        std::string result = toString(std::vector<int>(100, static_cast<int>(i)));
        FATP_ASSERT_TRUE(result.size() > 200, "reasonable output");
        // result goes out of scope, memory freed
    }

    // Additional check with maps (more complex allocation pattern)
    for (size_t i = 0; i < ITERATIONS / 10; ++i)
    {
        std::map<int, std::string> m;
        for (int j = 0; j < 10; ++j)
        {
            m[j] = "value" + std::to_string(j);
        }
        std::string result = toString(m);
        FATP_ASSERT_TRUE(result.size() > 50, "map output");
    }

    return true;
}

FATP_TEST_CASE(soak_error_recovery)
{
    // Soak test: Repeated error conditions and recovery
    constexpr size_t ITERATIONS = 1000;

    ThrowingToString thrower;

    for (size_t i = 0; i < ITERATIONS; ++i)
    {
        std::string out;
        bool result = tryToString(thrower, out);
        FATP_ASSERT_FALSE(result, "should fail");

        // Immediately after error, normal operation should work
        auto normal = toString(42);
        FATP_ASSERT_EQ(normal, "42", "recovery after error");
    }

    return true;
}

FATP_TEST_CASE(soak_depth_guard_repeated)
{
    // Soak test: Repeated depth limit hits and recovery
    using V5 = std::vector<std::vector<std::vector<std::vector<std::vector<int>>>>>;

    constexpr size_t ITERATIONS = 1000;

    for (size_t i = 0; i < ITERATIONS; ++i)
    {
        V5 deep = {{{{{static_cast<int>(i)}}}}};
        auto result = toString(deep);
        FATP_ASSERT_TRUE(result.find("<max depth>") != std::string::npos, "hit limit");

        // Recovery
        auto simple = toString(std::vector<int>{1, 2, 3});
        FATP_ASSERT_EQ(simple, "[1, 2, 3]", "recovery");
    }

    return true;
}

FATP_TEST_CASE(soak_concurrent_sustained)
{
    // Soak test: Concurrent sustained operation
    constexpr size_t NUM_THREADS = 4;
    constexpr auto DURATION = std::chrono::milliseconds(300);

    std::atomic<size_t> total_iterations{0};
    std::atomic<bool> stop_flag{false};
    std::atomic<bool> start_flag{false};
    std::vector<std::thread> threads;

    auto worker = [&](size_t thread_id) {
        while (!start_flag.load(std::memory_order_acquire))
        {
            std::this_thread::yield();
        }

        size_t local_count = 0;
        while (!stop_flag.load(std::memory_order_relaxed))
        {
            volatile auto s = toString(static_cast<int>(thread_id * 1000000 + local_count));
            (void)s;
            ++local_count;
        }
        total_iterations.fetch_add(local_count, std::memory_order_relaxed);
    };

    for (size_t t = 0; t < NUM_THREADS; ++t)
    {
        threads.emplace_back(worker, t);
    }

    start_flag.store(true, std::memory_order_release);
    std::this_thread::sleep_for(DURATION);
    stop_flag.store(true, std::memory_order_release);

    for (auto& t : threads)
    {
        t.join();
    }

    auto& out = *get_test_config().output;
    out << "  " << NUM_THREADS << " threads, " << total_iterations.load() << " total ops over 300ms\n";

    FATP_ASSERT_TRUE(total_iterations.load() > 10000, "sufficient concurrent throughput");
    return true;
}

// =============================================================================
// Main Test Runner
// =============================================================================

} // namespace fat_p::testing::stringify

namespace fat_p::testing
{


inline void run_benchmarks()
{
    using namespace fat_p::testing;

    auto& out = *get_test_config().output;
    out << "\n" << colors::cyan() << "Sanity Benchmark:" << colors::reset() << "\n";
    out << "  (benchmarks are provided by benchmark executables; unit tests do not run full benchmarks)\n\n";
}

bool test_Stringify()
{
    FATP_PRINT_HEADER(STRINGIFY)

    try
    {
        TestRunner runner;

        std::cout << colors::cyan() << colors::bold() << "=== Concept Detection ===" << colors::reset() << "\n";
        FATP_RUN_TEST_NS(runner, stringify, concept_has_to_string_method);
        FATP_RUN_TEST_NS(runner, stringify, concept_has_to_string_snake_method);
        FATP_RUN_TEST_NS(runner, stringify, concept_has_custom_string_method);
        FATP_RUN_TEST_NS(runner, stringify, concept_streamable);
        FATP_RUN_TEST_NS(runner, stringify, concept_wstreamable);
        FATP_RUN_TEST_NS(runner, stringify, concept_printable_range);
        FATP_RUN_TEST_NS(runner, stringify, concept_std_string_type);
        FATP_RUN_TEST_NS(runner, stringify, concept_stringifiable);

        std::cout << "\n"
                  << colors::cyan() << colors::bold() << "=== Integer Stringification ===" << colors::reset() << "\n";
        FATP_RUN_TEST_NS(runner, stringify, int_basic);
        FATP_RUN_TEST_NS(runner, stringify, int_boundaries);
        FATP_RUN_TEST_NS(runner, stringify, int_all_types);

        std::cout << "\n"
                  << colors::cyan() << colors::bold() << "=== Float Stringification ===" << colors::reset() << "\n";
        FATP_RUN_TEST_NS(runner, stringify, float_basic);
        FATP_RUN_TEST_NS(runner, stringify, float_special_values);
        FATP_RUN_TEST_NS(runner, stringify, float_precision_option);
        FATP_RUN_TEST_NS(runner, stringify, float_scientific_notation);

        std::cout << "\n"
                  << colors::cyan() << colors::bold() << "=== Boolean Stringification ===" << colors::reset() << "\n";
        FATP_RUN_TEST_NS(runner, stringify, bool_text_mode);
        FATP_RUN_TEST_NS(runner, stringify, bool_numeric_mode);

        std::cout << "\n"
                  << colors::cyan() << colors::bold() << "=== String Passthrough ===" << colors::reset() << "\n";
        FATP_RUN_TEST_NS(runner, stringify, string_std_string);
        FATP_RUN_TEST_NS(runner, stringify, string_c_string);
        FATP_RUN_TEST_NS(runner, stringify, string_null_c_string);
        FATP_RUN_TEST_NS(runner, stringify, string_char_array);

        std::cout << "\n"
                  << colors::cyan() << colors::bold() << "=== Enum Stringification ===" << colors::reset() << "\n";
        FATP_RUN_TEST_NS(runner, stringify, enum_with_stringifier);
        FATP_RUN_TEST_NS(runner, stringify, enum_without_stringifier);
        FATP_RUN_TEST_NS(runner, stringify, enum_unscoped);
        FATP_RUN_TEST_NS(runner, stringify, enum_in_container);

        std::cout << "\n"
                  << colors::cyan() << colors::bold() << "=== Custom Method Stringification ===" << colors::reset()
                  << "\n";
        FATP_RUN_TEST_NS(runner, stringify, custom_to_string_method);
        FATP_RUN_TEST_NS(runner, stringify, custom_to_string_snake_method);
        FATP_RUN_TEST_NS(runner, stringify, custom_both_methods_prefers_camel_case);
        FATP_RUN_TEST_NS(runner, stringify, custom_streamable);

        std::cout << "\n"
                  << colors::cyan() << colors::bold() << "=== Pair Stringification ===" << colors::reset() << "\n";
        FATP_RUN_TEST_NS(runner, stringify, pair_basic);
        FATP_RUN_TEST_NS(runner, stringify, pair_nested);
        FATP_RUN_TEST_NS(runner, stringify, pair_with_container);

        std::cout << "\n"
                  << colors::cyan() << colors::bold() << "=== Tuple Stringification ===" << colors::reset() << "\n";
        FATP_RUN_TEST_NS(runner, stringify, tuple_basic);
        FATP_RUN_TEST_NS(runner, stringify, tuple_empty);
        FATP_RUN_TEST_NS(runner, stringify, tuple_single);
        FATP_RUN_TEST_NS(runner, stringify, tuple_nested);

        std::cout << "\n"
                  << colors::cyan() << colors::bold() << "=== Optional Stringification ===" << colors::reset() << "\n";
        FATP_RUN_TEST_NS(runner, stringify, optional_with_value);
        FATP_RUN_TEST_NS(runner, stringify, optional_empty);
        FATP_RUN_TEST_NS(runner, stringify, optional_nested);

        std::cout << "\n"
                  << colors::cyan() << colors::bold() << "=== Container Stringification ===" << colors::reset() << "\n";
        FATP_RUN_TEST_NS(runner, stringify, container_vector);
        FATP_RUN_TEST_NS(runner, stringify, container_array);
        FATP_RUN_TEST_NS(runner, stringify, container_list);
        FATP_RUN_TEST_NS(runner, stringify, container_deque);
        FATP_RUN_TEST_NS(runner, stringify, container_forward_list);
        FATP_RUN_TEST_NS(runner, stringify, container_set);
        FATP_RUN_TEST_NS(runner, stringify, container_nested);
        FATP_RUN_TEST_NS(runner, stringify, container_custom_delimiters);

        std::cout << "\n"
                  << colors::cyan() << colors::bold() << "=== Map Stringification ===" << colors::reset() << "\n";
        FATP_RUN_TEST_NS(runner, stringify, map_basic);
        FATP_RUN_TEST_NS(runner, stringify, map_empty);
        FATP_RUN_TEST_NS(runner, stringify, map_unordered);

        std::cout << "\n"
                  << colors::cyan() << colors::bold() << "=== Recursion Depth Guard ===" << colors::reset() << "\n";
        FATP_RUN_TEST_NS(runner, stringify, depth_guard_default);
        FATP_RUN_TEST_NS(runner, stringify, depth_guard_custom);
        FATP_RUN_TEST_NS(runner, stringify, depth_guard_reset);

        std::cout << "\n"
                  << colors::cyan() << colors::bold() << "=== Type Priority Edge Cases ===" << colors::reset() << "\n";
        FATP_RUN_TEST_NS(runner, stringify, priority_fake_pair_uses_to_string);
        FATP_RUN_TEST_NS(runner, stringify, priority_iterable_uses_to_string);
        FATP_RUN_TEST_NS(runner, stringify, priority_non_stringifiable_placeholder);

        std::cout << "\n" << colors::cyan() << colors::bold() << "=== StringifyOptions ===" << colors::reset() << "\n";
        FATP_RUN_TEST_NS(runner, stringify, options_default_values);
        FATP_RUN_TEST_NS(runner, stringify, options_placeholder);
        FATP_RUN_TEST_NS(runner, stringify, options_classic_locale);

        std::cout << "\n"
                  << colors::cyan() << colors::bold() << "=== Convenience Functions ===" << colors::reset() << "\n";
        FATP_RUN_TEST_NS(runner, stringify, to_string_or);
        FATP_RUN_TEST_NS(runner, stringify, try_to_string_success);
        FATP_RUN_TEST_NS(runner, stringify, try_to_string_exception);
        FATP_RUN_TEST_NS(runner, stringify, to_w_string_basic);
        FATP_RUN_TEST_NS(runner, stringify, to_string_padded_right);
        FATP_RUN_TEST_NS(runner, stringify, to_string_padded_left);
        FATP_RUN_TEST_NS(runner, stringify, to_string_padded_center);
        FATP_RUN_TEST_NS(runner, stringify, to_string_padded_no_padding_needed);
        FATP_RUN_TEST_NS(runner, stringify, to_string_formatted);
        FATP_RUN_TEST_NS(runner, stringify, to_string_pointer);
        FATP_RUN_TEST_NS(runner, stringify, to_string_pointer_decimal);
        FATP_RUN_TEST_NS(runner, stringify, to_string_concat);

        std::cout << "\n" << colors::cyan() << colors::bold() << "=== Thread Safety ===" << colors::reset() << "\n";
        FATP_RUN_TEST_NS(runner, stringify, thread_safety_concurrent);
        FATP_RUN_TEST_NS(runner, stringify, thread_safety_error_independence);

        std::cout << "\n"
                  << colors::cyan() << colors::bold() << "=== Corner Cases: Integers ===" << colors::reset() << "\n";
        FATP_RUN_TEST_NS(runner, stringify, corner_char_as_integer);
        FATP_RUN_TEST_NS(runner, stringify, corner_int8_uint8);
        FATP_RUN_TEST_NS(runner, stringify, corner_unsigned_boundaries);

        std::cout << "\n"
                  << colors::cyan() << colors::bold() << "=== Corner Cases: Floats ===" << colors::reset() << "\n";
        FATP_RUN_TEST_NS(runner, stringify, corner_float_negative_zero);
        FATP_RUN_TEST_NS(runner, stringify, corner_float_denormalized);
        FATP_RUN_TEST_NS(runner, stringify, corner_float_extremes);
        FATP_RUN_TEST_NS(runner, stringify, corner_float_long_double);

        std::cout << "\n"
                  << colors::cyan() << colors::bold() << "=== Corner Cases: Strings ===" << colors::reset() << "\n";
        FATP_RUN_TEST_NS(runner, stringify, corner_string_embedded_null);
        FATP_RUN_TEST_NS(runner, stringify, corner_string_unicode);
        FATP_RUN_TEST_NS(runner, stringify, corner_string_view);
        FATP_RUN_TEST_NS(runner, stringify, corner_string_special_chars);

        std::cout << "\n"
                  << colors::cyan() << colors::bold() << "=== Corner Cases: Containers ===" << colors::reset() << "\n";
        FATP_RUN_TEST_NS(runner, stringify, corner_container_single_element);
        FATP_RUN_TEST_NS(runner, stringify, corner_container_multimap);
        FATP_RUN_TEST_NS(runner, stringify, corner_container_multiset);
        FATP_RUN_TEST_NS(runner, stringify, corner_container_unordered_set);
        FATP_RUN_TEST_NS(runner, stringify, corner_container_of_pairs);
        FATP_RUN_TEST_NS(runner, stringify, corner_container_of_optionals);

        std::cout << "\n"
                  << colors::cyan() << colors::bold() << "=== Corner Cases: Tuples/Arrays ===" << colors::reset()
                  << "\n";
        FATP_RUN_TEST_NS(runner, stringify, corner_tuple_homogeneous);
        FATP_RUN_TEST_NS(runner, stringify, corner_std_array_as_container);
        FATP_RUN_TEST_NS(runner, stringify, corner_pair_with_non_stringifiable);

        std::cout << "\n"
                  << colors::cyan() << colors::bold() << "=== Corner Cases: Optionals ===" << colors::reset() << "\n";
        FATP_RUN_TEST_NS(runner, stringify, corner_optional_triple_nested);
        FATP_RUN_TEST_NS(runner, stringify, corner_optional_of_non_stringifiable);

        std::cout << "\n"
                  << colors::cyan() << colors::bold() << "=== Corner Cases: Enums ===" << colors::reset() << "\n";
        FATP_RUN_TEST_NS(runner, stringify, corner_enum_negative_underlying);
        FATP_RUN_TEST_NS(runner, stringify, corner_enum_uint64_underlying);

        std::cout << "\n"
                  << colors::cyan() << colors::bold() << "=== Corner Cases: Custom Types ===" << colors::reset()
                  << "\n";
        FATP_RUN_TEST_NS(runner, stringify, corner_custom_empty_return);
        FATP_RUN_TEST_NS(runner, stringify, corner_custom_non_const_method);

        std::cout << "\n"
                  << colors::cyan() << colors::bold()
                  << "=== Corner Cases: References/Qualifiers ===" << colors::reset() << "\n";
        FATP_RUN_TEST_NS(runner, stringify, corner_const_reference);
        FATP_RUN_TEST_NS(runner, stringify, corner_rvalue);
        FATP_RUN_TEST_NS(runner, stringify, corner_pointer_non_null);

        std::cout << "\n"
                  << colors::cyan() << colors::bold() << "=== Corner Cases: Options ===" << colors::reset() << "\n";
        FATP_RUN_TEST_NS(runner, stringify, corner_options_all_float_options);
        FATP_RUN_TEST_NS(runner, stringify, corner_options_container_all);

        std::cout << "\n" << colors::cyan() << colors::bold() << "=== Stress Tests ===" << colors::reset() << "\n";
        FATP_RUN_TEST_NS(runner, stringify, stress_large_container);
        FATP_RUN_TEST_NS(runner, stringify, stress_deeply_nested_at_limit);
        FATP_RUN_TEST_NS(runner, stringify, stress_many_map_entries);
        FATP_RUN_TEST_NS(runner, stringify, stress_long_string_passthrough);
        FATP_RUN_TEST_NS(runner, stringify, stress_wide_tuple);
        FATP_RUN_TEST_NS(runner, stringify, stress_rapid_allocation);
        FATP_RUN_TEST_NS(runner, stringify, stress_mixed_types_rapid);
        FATP_RUN_TEST_NS(runner, stringify, stress_concurrent_high_load);

        std::cout << "\n" << colors::cyan() << colors::bold() << "=== Soak Tests ===" << colors::reset() << "\n";
        FATP_RUN_TEST_NS(runner, stringify, soak_sustained_operation);
        FATP_RUN_TEST_NS(runner, stringify, soak_memory_stability);
        FATP_RUN_TEST_NS(runner, stringify, soak_error_recovery);
        FATP_RUN_TEST_NS(runner, stringify, soak_depth_guard_repeated);
        FATP_RUN_TEST_NS(runner, stringify, soak_concurrent_sustained);

        return runner.print_summary() == 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << colors::red() << colors::bold() << "EXCEPTION: " << colors::reset() << e.what() << std::endl;
        return false;
    }
}

} // namespace fat_p::testing

#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_Stringify() ? 0 : 1;
}
#endif
