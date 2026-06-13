/**
 * @file test_Expected.cpp
 * @brief Comprehensive unit tests for Expected.h
 */
/*
FATP_META:
  meta_version: 1
  component: Expected
  file_role: test
  path: components/Expected/tests/test_Expected.cpp
  layer: Testing
  namespace: fat_p::testing::expected
  summary: "Unit tests for Expected."
  api_stability: in_work
  related:
    docs_search: "Expected"
    headers:
      - include/fat_p/Expected.h
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

#include <array>
#include <atomic>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "Expected.h"
#include "FatPTest.h"

namespace fat_p::testing::expected
{

// ============================================================================
// Helper Functions
// ============================================================================

Expected<int, std::string> divide(int a, int b)
{
    if (b == 0)
    {
        return unexpected{"Division by zero"};
    }
    return a / b;
}

Expected<int, std::string> safe_stoi(const std::string& s)
{
    try
    {
        return std::stoi(s);
    }
    catch (...)
    {
        return unexpected{"Invalid integer: " + s};
    }
}

Expected<void, std::string> validate_age(int age)
{
    if (age < 0 || age > 150)
    {
        return unexpected{"Invalid age"};
    }
    return {};
}

Expected<int, std::string> succeed_int(int v)
{
    return v;
}

Expected<int, std::string> fail_int(const std::string& msg)
{
    return unexpected{msg};
}

Expected<void, std::string> succeed_void()
{
    return {};
}

Expected<void, std::string> fail_void(const std::string& msg)
{
    return unexpected{msg};
}

// ============================================================================
// Unit Tests
// ============================================================================

FATP_TEST_CASE(basic_construction)
{
    Expected<int, std::string> v(42);
    FATP_ASSERT_TRUE(v.has_value(), "Expected should have value");
    FATP_ASSERT_TRUE(*v == 42, "Value should be 42");

    Expected<int, std::string> e(unexpected{"error"});
    FATP_ASSERT_TRUE(!e.has_value(), "Expected should have error");
    FATP_ASSERT_TRUE(e.error() == "error", "Error should be 'error'");

    return true;
}

FATP_TEST_CASE(copy_construction)
{
    Expected<int, std::string> v1(42);
    Expected<int, std::string> v2(v1);
    FATP_ASSERT_TRUE(*v2 == 42, "Copy construction should preserve value");

    Expected<std::string, int> s1("hello");
    Expected<std::string, int> s2(s1);
    FATP_ASSERT_TRUE(*s2 == "hello", "Copy construction should preserve string");

    return true;
}

FATP_TEST_CASE(move_construction)
{
    Expected<std::string, int> v1("hello");
    Expected<std::string, int> v2(std::move(v1));
    FATP_ASSERT_TRUE(*v2 == "hello", "Move construction should transfer value");

    return true;
}

FATP_TEST_CASE(copy_assignment)
{
    Expected<int, std::string> v1(42);
    Expected<int, std::string> v2(100);
    v1 = v2;
    FATP_ASSERT_TRUE(*v1 == 100, "Value should be 100 after assignment");

    Expected<int, std::string> v3(42);
    auto& self = v3;
    v3 = self;
    FATP_ASSERT_TRUE(*v3 == 42, "Self-assignment should work");

    return true;
}

FATP_TEST_CASE(move_assignment)
{
    Expected<std::string, int> v1("hello");
    Expected<std::string, int> v2("world");
    v2 = std::move(v1);
    FATP_ASSERT_TRUE(*v2 == "hello", "Move assignment should transfer value");

    return true;
}

FATP_TEST_CASE(value_access)
{
    Expected<int, std::string> v(42);
    FATP_ASSERT_TRUE(v.value() == 42, "value() should return 42");
    FATP_ASSERT_TRUE(*v == 42, "operator* should return 42");
    FATP_ASSERT_TRUE(v.value_or(0) == 42, "value_or should return 42");

    Expected<int, std::string> e(unexpected{"error"});
    FATP_ASSERT_TRUE(e.value_or(0) == 0, "value_or should return default");

    return true;
}

FATP_TEST_CASE(error_access)
{
    Expected<int, std::string> e(unexpected{"error"});
    FATP_ASSERT_TRUE(e.error() == "error", "error() should return 'error'");
    FATP_ASSERT_TRUE(e.error_or("default") == "error", "error_or should return error");

    Expected<int, std::string> v(42);
    FATP_ASSERT_TRUE(v.error_or("default") == "default", "error_or should return default");

    return true;
}

FATP_TEST_CASE(has_error)
{
    Expected<int, std::string> v(42);
    FATP_ASSERT_TRUE(!v.has_error(), "has_error should be false for value");
    FATP_ASSERT_TRUE(v.has_value() != v.has_error(), "has_value and has_error are opposites");

    Expected<int, std::string> e(unexpected{"error"});
    FATP_ASSERT_TRUE(e.has_error(), "has_error should be true for error");

    Expected<void, std::string> void_v;
    FATP_ASSERT_TRUE(!void_v.has_error(), "void has_error should be false for value");

    Expected<void, std::string> void_e(unexpected{"error"});
    FATP_ASSERT_TRUE(void_e.has_error(), "void has_error should be true for error");

    return true;
}

FATP_TEST_CASE(error_or_else)
{
    int call_count = 0;
    auto factory = [&]() {
        ++call_count;
        return std::string("computed");
    };

    Expected<int, std::string> v(42);
    std::string e1 = v.error_or_else(factory);
    FATP_ASSERT_TRUE(e1 == "computed", "error_or_else computes for value");
    FATP_ASSERT_TRUE(call_count == 1, "error_or_else calls factory for value");

    call_count = 0;
    Expected<int, std::string> e(unexpected{"actual"});
    std::string e2 = e.error_or_else(factory);
    FATP_ASSERT_TRUE(e2 == "actual", "error_or_else returns error");
    FATP_ASSERT_TRUE(call_count == 0, "error_or_else skips factory for error");

    return true;
}

FATP_TEST_CASE(map)
{
    auto result = Expected<int, std::string>(10).map([](int x) {
        return x * 2;
    });
    FATP_ASSERT_TRUE(*result == 20, "Map should double the value");

    auto err_result = Expected<int, std::string>(unexpected{"error"}).map([](int x) {
        return x * 2;
    });
    FATP_ASSERT_TRUE(!err_result.has_value(), "Error should propagate through map");
    FATP_ASSERT_TRUE(err_result.error() == "error", "Error should be preserved");

    return true;
}

FATP_TEST_CASE(and_then)
{
    auto result = Expected<int, std::string>(10).and_then([](int x) -> Expected<int, std::string> {
        return x * 2;
    });
    FATP_ASSERT_TRUE(*result == 20, "and_then should double the value");

    auto err_result = Expected<int, std::string>(unexpected{"error"}).and_then([](int x) -> Expected<int, std::string> {
        return x * 2;
    });
    FATP_ASSERT_TRUE(!err_result.has_value(), "Error should propagate through and_then");

    return true;
}

FATP_TEST_CASE(or_else)
{
    auto result =
        Expected<int, std::string>(unexpected{"error"}).or_else([](const std::string&) -> Expected<int, std::string> {
            return 42;
        });
    FATP_ASSERT_TRUE(*result == 42, "or_else should recover with 42");

    auto val_result = Expected<int, std::string>(10).or_else([](const std::string&) -> Expected<int, std::string> {
        return 42;
    });
    FATP_ASSERT_TRUE(*val_result == 10, "or_else should not affect values");

    return true;
}

FATP_TEST_CASE(transform_error)
{
    auto result = Expected<int, std::string>(unexpected{"error"}).transform_error([](const std::string& e) {
        return e + "_transformed";
    });
    FATP_ASSERT_TRUE(result.error() == "error_transformed", "transform_error should transform");

    return true;
}

FATP_TEST_CASE(inspect)
{
    int inspected_value = 0;
    Expected<int, std::string>(42).inspect([&](int x) {
        inspected_value = x;
    });
    FATP_ASSERT_TRUE(inspected_value == 42, "inspect should observe value");

    std::string inspected_error;
    Expected<int, std::string>(unexpected{"error"}).inspect_error([&](const std::string& e) {
        inspected_error = e;
    });
    FATP_ASSERT_TRUE(inspected_error == "error", "inspect_error should observe error");

    return true;
}

FATP_TEST_CASE(void_specialization)
{
    Expected<void, std::string> v;
    FATP_ASSERT_TRUE(v.has_value(), "Void Expected should have value");

    Expected<void, std::string> e(unexpected{"error"});
    FATP_ASSERT_TRUE(!e.has_value(), "Void Expected should have error");
    FATP_ASSERT_TRUE(e.error() == "error", "Void Expected error should be accessible");

    // Test map returning non-void
    auto mapped = v.map([]() {
        return 42;
    });
    FATP_ASSERT_TRUE(mapped.has_value() && *mapped == 42, "Void map to int works");

    // Test map returning void (void -> void)
    int side_effect = 0;
    auto void_mapped = v.map([&]() {
        side_effect = 100;
    });
    FATP_ASSERT_TRUE(void_mapped.has_value(), "Void map to void works");
    FATP_ASSERT_TRUE(side_effect == 100, "Void map side effect executed");

    // Test map on error state (should not invoke)
    side_effect = 0;
    auto err_mapped = e.map([&]() {
        side_effect = 999;
    });
    FATP_ASSERT_TRUE(!err_mapped.has_value(), "Error propagates through void map");
    FATP_ASSERT_TRUE(side_effect == 0, "Void map not invoked on error");

    return true;
}

FATP_TEST_CASE(emplace)
{
    Expected<std::string, int> exp(unexpected{42});
    exp.emplace("emplaced");
    FATP_ASSERT_TRUE(*exp == "emplaced", "Emplace should construct value");

    return true;
}

FATP_TEST_CASE(swap)
{
    Expected<int, std::string> v1(42);
    Expected<int, std::string> v2(100);
    v1.swap(v2);
    FATP_ASSERT_TRUE(*v1 == 100 && *v2 == 42, "Swap should exchange values");

    Expected<int, std::string> v(42);
    Expected<int, std::string> e(unexpected{"error"});
    v.swap(e);
    FATP_ASSERT_TRUE(!v.has_value() && e.has_value(), "Cross-state swap should work");
    FATP_ASSERT_TRUE(v.error() == "error" && *e == 42, "Cross-state swap data correct");

    return true;
}

FATP_TEST_CASE(comparisons)
{
    Expected<int, std::string> v1(42);
    Expected<int, std::string> v2(42);
    FATP_ASSERT_TRUE(v1 == v2, "Equal Expected should compare equal");
    FATP_ASSERT_TRUE(v1 == 42, "Expected should compare equal to value");

    Expected<int, std::string> e(unexpected{"error"});
    FATP_ASSERT_TRUE(e == unexpected{"error"}, "Expected should compare equal to unexpected");

    return true;
}

FATP_TEST_CASE(ordering)
{
    Expected<int, std::string> v1(5);
    Expected<int, std::string> v2(10);
    const Expected<int, std::string> v3(15);

    FATP_ASSERT_TRUE(v1 < v2, "operator< works");
    FATP_ASSERT_TRUE(v1 <= v2, "operator<= works");
    FATP_ASSERT_TRUE(v2 > v1, "operator> works");
    FATP_ASSERT_TRUE(v2 >= v1, "operator>= works");

    FATP_ASSERT_TRUE(v1 < v3, "operator< with const rhs");
    FATP_ASSERT_TRUE(v1 <= v3, "operator<= with const rhs");
    FATP_ASSERT_TRUE(v3 > v1, "operator> with const lhs");
    FATP_ASSERT_TRUE(v3 >= v1, "operator>= with const lhs");

    return true;
}

FATP_TEST_CASE(hash)
{
    Expected<int, std::string> v1(42);
    Expected<int, std::string> v2(42);
    std::hash<Expected<int, std::string>> hasher;

    FATP_ASSERT_TRUE(hasher(v1) == hasher(v2), "Same values should have same hash");

    return true;
}

FATP_TEST_CASE(make_expected)
{
    auto v1 = make_expected<std::string>(42);
    FATP_ASSERT_TRUE(v1.has_value() && *v1 == 42, "make_expected works");

    auto v2 = make_expected<int>(std::string("hello"));
    FATP_ASSERT_TRUE(*v2 == "hello", "make_expected different error type");

    return true;
}

FATP_TEST_CASE(result_status_aliases)
{
    Result<int> r = 42;
    FATP_ASSERT_TRUE(r.has_value() && *r == 42, "Result alias works");

    Status s;
    FATP_ASSERT_TRUE(s.has_value(), "Status default is success");

    Status s_err = unexpected{"failed"};
    FATP_ASSERT_TRUE(s_err.has_error(), "Status holds error");

    return true;
}

FATP_TEST_CASE(storage_policy)
{
    ExpectedUnion<int, std::string> v(42);
    FATP_ASSERT_TRUE(v.has_value(), "ExpectedUnion should have value");
    FATP_ASSERT_TRUE(*v == 42, "ExpectedUnion value should be 42");

    Expected<int, std::string> v2(100);
    FATP_ASSERT_TRUE(v2.has_value(), "Expected should have value");
    FATP_ASSERT_TRUE(*v2 == 100, "Expected value should be 100");

    return true;
}

FATP_TEST_CASE(rebind)
{
    using IntExpected = Expected<int, std::string>;
    using DoubleExpected = IntExpected::rebind<double>;

    static_assert(std::is_same_v<DoubleExpected, Expected<double, std::string>>, "rebind changes value type");

    auto to_double = [](auto exp) -> typename decltype(exp)::template rebind<double> {
        return exp.map([](const auto& x) {
            return static_cast<double>(x);
        });
    };

    Expected<int, std::string> int_exp(42);
    auto double_exp = to_double(int_exp);
    FATP_ASSERT_TRUE(double_exp.has_value(), "Rebind conversion works");
    FATP_ASSERT_TRUE(*double_exp == 42.0, "Rebind value correct");

    return true;
}

FATP_TEST_CASE(non_default_constructible)
{
    struct NoDefault
    {
        int value;
        NoDefault() = delete;
        explicit NoDefault(int v)
            : value(v)
        {
        }
    };

    Expected<NoDefault, std::string> exp(std::in_place, 42);
    FATP_ASSERT_TRUE(exp.has_value(), "NoDefault construction works");
    FATP_ASSERT_TRUE(exp->value == 42, "NoDefault value correct");

    Expected<NoDefault, std::string> err(unexpected{"error"});
    FATP_ASSERT_TRUE(!err.has_value(), "NoDefault error state works");

    return true;
}

FATP_TEST_CASE(large_objects)
{
    struct LargeObject
    {
        std::array<int, 100> data;
        LargeObject()
        {
            data.fill(42);
        }
    };

    Expected<LargeObject, std::string> exp(std::in_place);
    FATP_ASSERT_TRUE(exp.has_value(), "Large object construction");
    FATP_ASSERT_TRUE(exp->data[0] == 42, "Large object value correct");

    auto moved = std::move(exp);
    FATP_ASSERT_TRUE(moved.has_value(), "Large object moved");

    return true;
}

FATP_TEST_CASE(concurrent_read)
{
    Expected<int, std::string> shared_exp(42);
    std::atomic<int> sum{0};

    std::vector<std::thread> threads;
    for (int i = 0; i < 4; ++i)
    {
        threads.emplace_back([&]() {
            for (int j = 0; j < 100; ++j)
            {
                if (shared_exp.has_value())
                {
                    sum += *shared_exp;
                }
            }
        });
    }

    for (auto& t : threads)
    {
        t.join();
    }

    FATP_ASSERT_TRUE(sum == 42 * 400, "Concurrent reads safe");

    return true;
}

FATP_TEST_CASE(monadic_chaining)
{
    auto result = safe_stoi("10")
                      .and_then([](int x) {
                          return divide(100, x);
                      })
                      .map([](int x) {
                          return x * 2;
                      });

    FATP_ASSERT_TRUE(result.has_value(), "Chained operations succeed");
    FATP_ASSERT_TRUE(*result == 20, "Chained result correct");

    auto err_result = safe_stoi("not_a_number")
                          .and_then([](int x) {
                              return divide(100, x);
                          })
                          .map([](int x) {
                              return x * 2;
                          });

    FATP_ASSERT_TRUE(!err_result.has_value(), "Error propagates through chain");

    return true;
}

FATP_TEST_CASE(fold)
{
    auto v = Expected<int, std::string>(42);
    int result = v.fold(
        [](int x) {
            return x * 2;
        },
        [](const std::string&) {
            return -1;
        });
    FATP_ASSERT_TRUE(result == 84, "fold with value");

    auto e = Expected<int, std::string>(unexpected{"error"});
    int err_result = e.fold(
        [](int x) {
            return x * 2;
        },
        [](const std::string&) {
            return -1;
        });
    FATP_ASSERT_TRUE(err_result == -1, "fold with error");

    return true;
}

FATP_TEST_CASE(value_unchecked)
{
    Expected<int, std::string> v(42);
    FATP_ASSERT_TRUE(v.value_unchecked() == 42, "value_unchecked returns value");

    v.value_unchecked() = 100;
    FATP_ASSERT_TRUE(*v == 100, "value_unchecked can modify");

    const Expected<int, std::string> cv(200);
    FATP_ASSERT_TRUE(cv.value_unchecked() == 200, "const value_unchecked works");

    Expected<std::string, int> sv("hello");
    FATP_ASSERT_TRUE(sv.value_unchecked() == "hello", "value_unchecked with string");

    return true;
}

FATP_TEST_CASE(trivial_storage)
{
    enum class ErrorCode : int
    {
        None = 0,
        NotFound = 1,
        Invalid = 2
    };
    using TrivExp = fat_p::ExpectedImpl<int, ErrorCode, fat_p::TrivialStorage>;

    TrivExp v(42);
    FATP_ASSERT_TRUE(v.has_value(), "TrivialExpected has value");
    FATP_ASSERT_TRUE(*v == 42, "TrivialExpected value correct");

    TrivExp e(fat_p::unexpect, ErrorCode::NotFound);
    FATP_ASSERT_TRUE(!e.has_value(), "TrivialExpected has error");
    FATP_ASSERT_TRUE(e.error() == ErrorCode::NotFound, "TrivialExpected error correct");

    TrivExp copy = v;
    FATP_ASSERT_TRUE(*copy == 42, "TrivialExpected copy works");

    TrivExp moved = std::move(copy);
    FATP_ASSERT_TRUE(*moved == 42, "TrivialExpected move works");

    static_assert(std::is_trivially_copyable_v<fat_p::TrivialStorage<int, ErrorCode>>,
                  "TrivialStorage should be trivially copyable");

    static_assert(std::is_trivially_copyable_v<TrivExp>,
                  "TrivialExpected should be trivially copyable for register passing");

    v.swap(e);
    FATP_ASSERT_TRUE(!v.has_value() && e.has_value(), "TrivialExpected swap works");

    auto mapped = e.map([](int x) {
        return x * 2;
    });
    FATP_ASSERT_TRUE(*mapped == 84, "TrivialExpected map works");

    return true;
}

FATP_TEST_CASE(assign_or_return)
{
    auto success_fn = []() -> Expected<int, std::string> {
        return 42;
    };
    auto fail_fn = []() -> Expected<int, std::string> {
        return unexpected{"fail"};
    };

    auto wrapper_success = [&]() -> Expected<int, std::string> {
        int val = 0;
        FATP_EXPECTED_ASSIGN_OR_RETURN(val, success_fn());
        return val * 2;
    };

    auto wrapper_fail = [&]() -> Expected<int, std::string> {
        int val = 0;
        FATP_EXPECTED_ASSIGN_OR_RETURN(val, fail_fn());
        return val * 2;
    };

    auto res1 = wrapper_success();
    FATP_ASSERT_TRUE(res1.has_value() && *res1 == 84, "ASSIGN_OR_RETURN success path");

    auto res2 = wrapper_fail();
    FATP_ASSERT_TRUE(!res2.has_value() && res2.error() == "fail", "ASSIGN_OR_RETURN error propagation");

    return true;
}

#if defined(__cpp_lib_three_way_comparison) && __cpp_lib_three_way_comparison >= 201907L

FATP_TEST_CASE(map_to_void)
{
    bool called = false;
    Expected<int, std::string> value(7);
    auto mapped = value.map([&](int x) {
        called = (x == 7);
    });

    FATP_ASSERT_TRUE(called, "void-returning map should invoke callable for value state");
    FATP_ASSERT_TRUE(mapped.has_value(), "void-returning map should return successful Expected<void,E>");

    Expected<int, std::string> error(unexpected<std::string>("bad"));
    auto propagated = error.map([](int) {});
    FATP_ASSERT_TRUE(propagated.has_error(), "void-returning map should propagate error state");
    FATP_ASSERT_EQ(propagated.error(), std::string("bad"), "void-returning map should preserve error");

    return true;
}

FATP_TEST_CASE(void_unexpect_default_error)
{
    Expected<void, int> status(unexpect);
    FATP_ASSERT_TRUE(status.has_error(), "Expected<void,int>(unexpect) should be an error");
    FATP_ASSERT_EQ(status.error(), 0, "Default-constructed int error should be value-initialized");
    return true;
}

FATP_TEST_CASE(converting_constructors)
{
    Expected<int, const char*> value(42);
    Expected<long, std::string> converted_value(value);
    FATP_ASSERT_TRUE(converted_value.has_value(), "Converted value should remain a value");
    FATP_ASSERT_EQ(*converted_value, 42L, "Converted value should preserve payload");

    Expected<int, const char*> error(unexpected<const char*>("converted error"));
    Expected<long, std::string> converted_error(error);
    FATP_ASSERT_TRUE(converted_error.has_error(), "Converted error should remain an error");
    FATP_ASSERT_EQ(converted_error.error(), std::string("converted error"), "Converted error should preserve payload");

    return true;
}

FATP_TEST_CASE(trivial_default_and_void_map)
{
    TrivialExpected<int, int> value;
    FATP_ASSERT_TRUE(value.has_value(), "Default TrivialExpected should hold a value");
    FATP_ASSERT_EQ(*value, 0, "Default TrivialExpected<int,int> should value-initialize int");

    auto mapped = value.map([](int) {});
    FATP_ASSERT_TRUE(mapped.has_value(), "TrivialExpected void map should succeed for value state");

    TrivialExpected<int, int> error(unexpect, -7);
    auto propagated = error.map([](int) {});
    FATP_ASSERT_TRUE(propagated.has_error(), "TrivialExpected void map should propagate error state");
    FATP_ASSERT_EQ(propagated.error(), -7, "TrivialExpected void map should preserve error");

    return true;
}

FATP_TEST_CASE(three_way_comparison)
{
    Expected<int, std::string> v1(42);
    Expected<int, std::string> v2(43);
    Expected<int, std::string> v3(42);
    Expected<int, std::string> err1(unexpected{"error1"});
    Expected<int, std::string> err2(unexpected{"error2"});

    FATP_ASSERT_TRUE((v1 <=> v2) == std::strong_ordering::less, "42 < 43");
    FATP_ASSERT_TRUE((v2 <=> v1) == std::strong_ordering::greater, "43 > 42");
    FATP_ASSERT_TRUE((v1 <=> v3) == std::strong_ordering::equal, "42 == 42");
    FATP_ASSERT_TRUE((err1 <=> v1) == std::strong_ordering::less, "error < value");
    FATP_ASSERT_TRUE((v1 <=> err1) == std::strong_ordering::greater, "value > error");
    FATP_ASSERT_TRUE((err1 <=> err2) == std::strong_ordering::less, "error1 < error2");

    return true;
}
#endif

#if defined(__cpp_lib_expected) && __cpp_lib_expected >= 202202L
FATP_TEST_CASE(std_expected_integration)
{
    Expected<int, std::string> custom(42);
    auto std_exp = to_std_expected(custom);

    FATP_ASSERT_TRUE(std_exp.has_value(), "Converted value state");
    FATP_ASSERT_TRUE(*std_exp == 42, "Converted value correct");

    Expected<int, std::string> custom_err(unexpected{"error"});
    auto std_exp_err = to_std_expected(custom_err);

    FATP_ASSERT_TRUE(!std_exp_err.has_value(), "Converted error state");
    FATP_ASSERT_TRUE(std_exp_err.error() == "error", "Converted error correct");

    std::expected<int, std::string> std_v(42);
    auto back = from_std_expected(std_v);

    FATP_ASSERT_TRUE(back.has_value(), "Converted back value state");
    FATP_ASSERT_TRUE(*back == 42, "Converted back value correct");

    Expected<void, std::string> custom_void;
    auto std_void = to_std_expected(custom_void);
    FATP_ASSERT_TRUE(std_void.has_value(), "Void conversion works");

    return true;
}
#endif

// ============================================================================
// Coverage Gap Tests — bad_expected_access
// ============================================================================

FATP_TEST_CASE(bad_expected_access_from_value)
{
    Expected<int, std::string> e(unexpected{"access denied"});
    bool caught = false;
    try
    {
        (void)e.value();
    }
    catch (const bad_expected_access<std::string>& ex)
    {
        caught = true;
        FATP_ASSERT_EQ(ex.error(), std::string("access denied"),
                        "Exception must preserve the error payload");
        FATP_ASSERT_TRUE(ex.what() != nullptr, "what() must return non-null");
    }
    FATP_ASSERT_TRUE(caught, "value() on error state must throw bad_expected_access");
    return true;
}

FATP_TEST_CASE(bad_expected_access_rvalue_error)
{
    Expected<int, std::string> e(unexpected{"moveable"});
    bool caught = false;
    try
    {
        (void)std::move(e).value();
    }
    catch (bad_expected_access<std::string>& ex)
    {
        caught = true;
        std::string moved_out = std::move(ex).error();
        FATP_ASSERT_EQ(moved_out, std::string("moveable"),
                        "Rvalue error() on exception must yield the payload");
    }
    FATP_ASSERT_TRUE(caught, "Rvalue value() on error state must throw");
    return true;
}

FATP_TEST_CASE(bad_expected_access_void)
{
    Expected<void, std::string> e(unexpected{"void access"});
    bool caught = false;
    try
    {
        e.value();
    }
    catch (const bad_expected_access<std::string>& ex)
    {
        caught = true;
        FATP_ASSERT_EQ(ex.error(), std::string("void access"),
                        "Void Expected value() must throw with error");
    }
    FATP_ASSERT_TRUE(caught, "Void Expected value() on error state must throw");
    return true;
}

// ============================================================================
// Coverage Gap Tests — FATP_EXPECTED_TRY / TRY_VOID
// ============================================================================

FATP_TEST_CASE(expected_try_success)
{
    auto outer = []() -> Expected<int, std::string> {
        FATP_EXPECTED_TRY(a, succeed_int(10));
        FATP_EXPECTED_TRY(b, succeed_int(20));
        return a + b;
    };

    auto result = outer();
    FATP_ASSERT_TRUE(result.has_value(), "TRY success path should yield value");
    FATP_ASSERT_EQ(*result, 30, "TRY should bind both variables");
    return true;
}

FATP_TEST_CASE(expected_try_propagates_error)
{
    auto outer = []() -> Expected<int, std::string> {
        FATP_EXPECTED_TRY(a, succeed_int(10));
        FATP_EXPECTED_TRY(b, fail_int("second failed"));
        return a + b;
    };

    auto result = outer();
    FATP_ASSERT_TRUE(!result.has_value(), "TRY must propagate error");
    FATP_ASSERT_EQ(result.error(), std::string("second failed"),
                    "TRY must forward the original error message");
    return true;
}

FATP_TEST_CASE(expected_try_void_success)
{
    int side_effect = 0;
    auto void_work = [&]() -> Expected<void, std::string> {
        side_effect = 99;
        return {};
    };

    auto outer = [&]() -> Expected<int, std::string> {
        FATP_EXPECTED_TRY_VOID(void_work());
        return side_effect;
    };

    auto result = outer();
    FATP_ASSERT_TRUE(result.has_value(), "TRY_VOID success path should continue");
    FATP_ASSERT_EQ(*result, 99, "Side effect should have executed");
    return true;
}

FATP_TEST_CASE(expected_try_void_propagates_error)
{
    auto outer = []() -> Expected<int, std::string> {
        FATP_EXPECTED_TRY_VOID(fail_void("void failed"));
        return 42;
    };

    auto result = outer();
    FATP_ASSERT_TRUE(!result.has_value(), "TRY_VOID must propagate error");
    FATP_ASSERT_EQ(result.error(), std::string("void failed"),
                    "TRY_VOID must forward the original error");
    return true;
}

// ============================================================================
// Coverage Gap Tests — value_or_else
// ============================================================================

FATP_TEST_CASE(value_or_else_value_path)
{
    int factory_calls = 0;
    auto factory = [&]() {
        ++factory_calls;
        return -1;
    };

    Expected<int, std::string> v(42);
    int result = v.value_or_else(factory);
    FATP_ASSERT_EQ(result, 42, "value_or_else returns value when present");
    FATP_ASSERT_EQ(factory_calls, 0, "Factory must not be called on value path");
    return true;
}

FATP_TEST_CASE(value_or_else_error_path)
{
    int factory_calls = 0;
    auto factory = [&]() {
        ++factory_calls;
        return 999;
    };

    Expected<int, std::string> e(unexpected{"err"});
    int result = e.value_or_else(factory);
    FATP_ASSERT_EQ(result, 999, "value_or_else returns factory result on error");
    FATP_ASSERT_EQ(factory_calls, 1, "Factory must be called exactly once");
    return true;
}

FATP_TEST_CASE(value_or_else_rvalue)
{
    auto result = Expected<std::string, int>(unexpected{-1}).value_or_else([]() {
        return std::string("fallback");
    });
    FATP_ASSERT_EQ(result, std::string("fallback"),
                    "Rvalue value_or_else should work");
    return true;
}

// ============================================================================
// Coverage Gap Tests — Void Specialization
// ============================================================================

FATP_TEST_CASE(void_emplace)
{
    Expected<void, std::string> e(unexpected{"was error"});
    FATP_ASSERT_TRUE(e.has_error(), "Precondition: error state");

    e.emplace();
    FATP_ASSERT_TRUE(e.has_value(), "emplace() must transition to value state");
    return true;
}

FATP_TEST_CASE(void_swap)
{
    Expected<void, std::string> v;
    Expected<void, std::string> e(unexpected{"swap me"});

    v.swap(e);
    FATP_ASSERT_TRUE(!v.has_value(), "After swap, v should hold error");
    FATP_ASSERT_TRUE(e.has_value(), "After swap, e should hold value");
    FATP_ASSERT_EQ(v.error(), std::string("swap me"), "Error payload preserved");

    Expected<void, std::string> e1(unexpected{"alpha"});
    Expected<void, std::string> e2(unexpected{"beta"});
    e1.swap(e2);
    FATP_ASSERT_EQ(e1.error(), std::string("beta"), "Error-error swap A");
    FATP_ASSERT_EQ(e2.error(), std::string("alpha"), "Error-error swap B");
    return true;
}

FATP_TEST_CASE(void_copy_assignment)
{
    Expected<void, std::string> v;
    Expected<void, std::string> e(unexpected{"assign me"});

    v = e;
    FATP_ASSERT_TRUE(!v.has_value(), "Copy assignment value->error");
    FATP_ASSERT_EQ(v.error(), std::string("assign me"), "Error preserved");

    Expected<void, std::string> v2;
    e = v2;
    FATP_ASSERT_TRUE(e.has_value(), "Copy assignment error->value");
    return true;
}

FATP_TEST_CASE(void_move_assignment)
{
    Expected<void, std::string> v;
    Expected<void, std::string> e(unexpected{"move me"});

    v = std::move(e);
    FATP_ASSERT_TRUE(!v.has_value(), "Move assignment value->error");
    FATP_ASSERT_EQ(v.error(), std::string("move me"), "Error preserved");
    return true;
}

FATP_TEST_CASE(void_and_then)
{
    Expected<void, std::string> v;
    auto result = v.and_then([]() -> Expected<int, std::string> {
        return 42;
    });
    FATP_ASSERT_TRUE(result.has_value(), "void and_then should invoke on value");
    FATP_ASSERT_EQ(*result, 42, "void and_then should return continuation result");

    Expected<void, std::string> e(unexpected{"blocked"});
    auto err_result = e.and_then([]() -> Expected<int, std::string> {
        return 42;
    });
    FATP_ASSERT_TRUE(!err_result.has_value(), "void and_then should propagate error");
    FATP_ASSERT_EQ(err_result.error(), std::string("blocked"), "Error preserved");
    return true;
}

FATP_TEST_CASE(void_or_else)
{
    Expected<void, std::string> e(unexpected{"recover me"});
    auto recovered = e.or_else([](const std::string&) -> Expected<void, std::string> {
        return {};
    });
    FATP_ASSERT_TRUE(recovered.has_value(), "void or_else should recover");

    Expected<void, std::string> v;
    auto pass = v.or_else([](const std::string&) -> Expected<void, std::string> {
        return unexpected{"should not happen"};
    });
    FATP_ASSERT_TRUE(pass.has_value(), "void or_else should pass-through value");
    return true;
}

FATP_TEST_CASE(void_map_error)
{
    Expected<void, std::string> e(unexpected{"original"});
    auto transformed = e.map_error([](const std::string& s) {
        return static_cast<int>(s.size());
    });
    FATP_ASSERT_TRUE(!transformed.has_value(), "map_error should keep error state");
    FATP_ASSERT_EQ(transformed.error(), 8, "map_error should transform error value");

    Expected<void, std::string> v;
    auto pass = v.map_error([](const std::string&) {
        return -1;
    });
    FATP_ASSERT_TRUE(pass.has_value(), "map_error should pass-through value");
    return true;
}

FATP_TEST_CASE(void_inspect)
{
    int value_inspections = 0;
    int error_inspections = 0;

    Expected<void, std::string> v;
    v.inspect([&]() { ++value_inspections; });
    v.inspect_error([&](const std::string&) { ++error_inspections; });

    FATP_ASSERT_EQ(value_inspections, 1, "inspect should fire on value state");
    FATP_ASSERT_EQ(error_inspections, 0, "inspect_error should not fire on value state");

    Expected<void, std::string> e(unexpected{"err"});
    e.inspect([&]() { ++value_inspections; });
    e.inspect_error([&](const std::string&) { ++error_inspections; });

    FATP_ASSERT_EQ(value_inspections, 1, "inspect should not fire on error state");
    FATP_ASSERT_EQ(error_inspections, 1, "inspect_error should fire on error state");
    return true;
}

FATP_TEST_CASE(void_fold)
{
    Expected<void, std::string> v;
    int r1 = v.fold(
        []() { return 1; },
        [](const std::string&) { return -1; });
    FATP_ASSERT_EQ(r1, 1, "void fold value path");

    Expected<void, std::string> e(unexpected{"err"});
    int r2 = e.fold(
        []() { return 1; },
        [](const std::string&) { return -1; });
    FATP_ASSERT_EQ(r2, -1, "void fold error path");
    return true;
}

FATP_TEST_CASE(void_error_or_else)
{
    int factory_calls = 0;
    auto factory = [&]() {
        ++factory_calls;
        return std::string("computed");
    };

    Expected<void, std::string> v;
    std::string r1 = v.error_or_else(factory);
    FATP_ASSERT_EQ(r1, std::string("computed"), "error_or_else computes for value");
    FATP_ASSERT_EQ(factory_calls, 1, "Factory called once");

    factory_calls = 0;
    Expected<void, std::string> e(unexpected{"actual"});
    std::string r2 = e.error_or_else(factory);
    FATP_ASSERT_EQ(r2, std::string("actual"), "error_or_else returns error");
    FATP_ASSERT_EQ(factory_calls, 0, "Factory not called for error");
    return true;
}

FATP_TEST_CASE(void_comparison)
{
    Expected<void, std::string> v1;
    Expected<void, std::string> v2;
    Expected<void, std::string> e1(unexpected{"a"});
    Expected<void, std::string> e2(unexpected{"a"});
    Expected<void, std::string> e3(unexpected{"b"});

    FATP_ASSERT_TRUE(v1 == v2, "Two void values are equal");
    FATP_ASSERT_TRUE(!(v1 == e1), "Void value != error");
    FATP_ASSERT_TRUE(e1 == e2, "Same errors are equal");
    FATP_ASSERT_TRUE(!(e1 == e3), "Different errors are not equal");
    FATP_ASSERT_TRUE(e1 == unexpected{"a"}, "Void Expected == unexpected");
    return true;
}

// ============================================================================
// Coverage Gap Tests — Rvalue-qualified Accessors
// ============================================================================

FATP_TEST_CASE(rvalue_value_access)
{
    auto val = Expected<std::string, int>("rvalue test").value();
    FATP_ASSERT_EQ(val, std::string("rvalue test"), "Rvalue value() returns correctly");

    auto val2 = *Expected<std::string, int>("deref test");
    FATP_ASSERT_EQ(val2, std::string("deref test"), "Rvalue operator* returns correctly");

    auto val3 = Expected<std::string, int>(unexpected{-1}).value_or("fallback");
    FATP_ASSERT_EQ(val3, std::string("fallback"), "Rvalue value_or returns default");

    auto val4 = Expected<std::string, int>("unchecked").value_unchecked();
    FATP_ASSERT_EQ(val4, std::string("unchecked"), "Rvalue value_unchecked works");
    return true;
}

FATP_TEST_CASE(rvalue_error_access)
{
    auto err = Expected<int, std::string>(unexpected{"rvalue err"}).error();
    FATP_ASSERT_EQ(err, std::string("rvalue err"), "Rvalue error() returns correctly");

    auto err2 = Expected<int, std::string>(42).error_or("default");
    FATP_ASSERT_EQ(err2, std::string("default"), "Rvalue error_or returns default");
    return true;
}

FATP_TEST_CASE(rvalue_move_semantics)
{
    auto exp = Expected<std::unique_ptr<int>, std::string>(
        std::in_place, std::make_unique<int>(77));
    FATP_ASSERT_TRUE(exp.has_value(), "Precondition: has value");

    auto ptr = std::move(exp).value();
    FATP_ASSERT_TRUE(ptr != nullptr, "Moved-out ptr should be valid");
    FATP_ASSERT_EQ(*ptr, 77, "Moved-out value should be 77");
    return true;
}

// ============================================================================
// Coverage Gap Tests — operator bool
// ============================================================================

FATP_TEST_CASE(explicit_bool_conversion)
{
    Expected<int, std::string> v(42);
    Expected<int, std::string> e(unexpected{"err"});
    Expected<void, std::string> vv;
    Expected<void, std::string> ve(unexpected{"err"});

    FATP_ASSERT_TRUE(static_cast<bool>(v), "Value Expected is true");
    FATP_ASSERT_TRUE(!static_cast<bool>(e), "Error Expected is false");
    FATP_ASSERT_TRUE(static_cast<bool>(vv), "Void value Expected is true");
    FATP_ASSERT_TRUE(!static_cast<bool>(ve), "Void error Expected is false");

    if (v) { /* OK */ }
    else
    {
        FATP_ASSERT_TRUE(false, "Value should be truthy in if-statement");
    }
    return true;
}

// ============================================================================
// Coverage Gap Tests — Monadic Aliases
// ============================================================================

FATP_TEST_CASE(transform_alias)
{
    auto result = Expected<int, std::string>(5).transform([](int x) {
        return x * 3;
    });
    FATP_ASSERT_TRUE(result.has_value(), "transform should work like map");
    FATP_ASSERT_EQ(*result, 15, "transform result correct");

    auto err = Expected<int, std::string>(unexpected{"e"}).transform([](int x) {
        return x * 3;
    });
    FATP_ASSERT_TRUE(!err.has_value(), "transform should propagate error");
    return true;
}

FATP_TEST_CASE(flat_map_alias)
{
    auto result = Expected<int, std::string>(10).flat_map([](int x) -> Expected<int, std::string> {
        return x + 5;
    });
    FATP_ASSERT_TRUE(result.has_value(), "flat_map should work like and_then");
    FATP_ASSERT_EQ(*result, 15, "flat_map result correct");

    auto err = Expected<int, std::string>(unexpected{"e"}).flat_map([](int x) -> Expected<int, std::string> {
        return x + 5;
    });
    FATP_ASSERT_TRUE(!err.has_value(), "flat_map should propagate error");
    return true;
}

FATP_TEST_CASE(map_error_direct)
{
    auto result = Expected<int, std::string>(unexpected{"err"}).map_error([](const std::string& e) {
        return static_cast<int>(e.size());
    });
    FATP_ASSERT_TRUE(!result.has_value(), "map_error keeps error state");
    FATP_ASSERT_EQ(result.error(), 3, "map_error transforms error");

    auto pass = Expected<int, std::string>(42).map_error([](const std::string&) {
        return -1;
    });
    FATP_ASSERT_TRUE(pass.has_value(), "map_error passes through value");
    FATP_ASSERT_EQ(*pass, 42, "map_error preserves value");
    return true;
}

// ============================================================================
// Coverage Gap Tests — unexpected Standalone Operations
// ============================================================================

FATP_TEST_CASE(unexpected_standalone_ops)
{
    unexpected<std::string> u("test");
    FATP_ASSERT_EQ(u.value(), std::string("test"), "Lvalue value()");

    const unexpected<std::string> cu("const");
    FATP_ASSERT_EQ(cu.value(), std::string("const"), "Const lvalue value()");

    auto rv = unexpected<std::string>("rval").value();
    FATP_ASSERT_EQ(rv, std::string("rval"), "Rvalue value()");

    unexpected<std::string> a("alpha");
    unexpected<std::string> b("beta");
    a.swap(b);
    FATP_ASSERT_EQ(a.value(), std::string("beta"), "swap a");
    FATP_ASSERT_EQ(b.value(), std::string("alpha"), "swap b");

    unexpected<int> x(1);
    unexpected<int> y(1);
    unexpected<int> z(2);
    FATP_ASSERT_TRUE(x == y, "Equal unexpected values compare equal");
    FATP_ASSERT_TRUE(!(x == z), "Different unexpected values not equal");

    swap(a, b);
    FATP_ASSERT_EQ(a.value(), std::string("alpha"), "Free swap a");
    FATP_ASSERT_EQ(b.value(), std::string("beta"), "Free swap b");
    return true;
}

FATP_TEST_CASE(make_unexpected_factory)
{
    auto u = make_unexpected(42);
    static_assert(std::is_same_v<decltype(u), unexpected<int>>,
                  "make_unexpected should deduce unexpected<int>");
    FATP_ASSERT_EQ(u.value(), 42, "make_unexpected preserves value");

    auto us = make_unexpected(std::string("factory"));
    FATP_ASSERT_EQ(us.value(), std::string("factory"), "make_unexpected string");
    return true;
}

// ============================================================================
// Coverage Gap Tests — CTAD Deduction Guides
// ============================================================================

FATP_TEST_CASE(ctad_deduction)
{
    ExpectedImpl value_exp(42);
    static_assert(std::is_same_v<decltype(value_exp),
                                  ExpectedImpl<int, std::string, UnionStorage>>,
                  "CTAD from value should deduce <int, string, UnionStorage>");
    FATP_ASSERT_TRUE(value_exp.has_value(), "CTAD value has value");
    FATP_ASSERT_EQ(*value_exp, 42, "CTAD value correct");

    ExpectedImpl error_exp(unexpected{-1});
    static_assert(std::is_same_v<decltype(error_exp),
                                  ExpectedImpl<void, int, UnionStorage>>,
                  "CTAD from unexpected should deduce <void, int, UnionStorage>");
    FATP_ASSERT_TRUE(!error_exp.has_value(), "CTAD error has error");
    FATP_ASSERT_EQ(error_exp.error(), -1, "CTAD error correct");
    return true;
}

// ============================================================================
// Coverage Gap Tests — Initializer-List Constructors
// ============================================================================

FATP_TEST_CASE(initializer_list_value_construction)
{
    Expected<std::vector<int>, std::string> exp(std::in_place, {1, 2, 3});
    FATP_ASSERT_TRUE(exp.has_value(), "Init-list in_place should construct value");
    FATP_ASSERT_EQ(exp->size(), 3u, "Vector should have 3 elements");
    FATP_ASSERT_EQ((*exp)[0], 1, "First element correct");
    FATP_ASSERT_EQ((*exp)[2], 3, "Third element correct");
    return true;
}

FATP_TEST_CASE(initializer_list_error_construction)
{
    Expected<int, std::vector<int>> exp(unexpect, {10, 20, 30});
    FATP_ASSERT_TRUE(!exp.has_value(), "Init-list unexpect should construct error");
    FATP_ASSERT_EQ(exp.error().size(), 3u, "Error vector should have 3 elements");
    FATP_ASSERT_EQ(exp.error()[1], 20, "Second error element correct");
    return true;
}

FATP_TEST_CASE(initializer_list_emplace)
{
    Expected<std::vector<int>, std::string> exp(unexpected{"was error"});
    exp.emplace({4, 5, 6});
    FATP_ASSERT_TRUE(exp.has_value(), "Emplace with init-list should set value");
    FATP_ASSERT_EQ(exp->size(), 3u, "Emplaced vector has 3 elements");
    FATP_ASSERT_EQ((*exp)[0], 4, "First emplaced element correct");
    return true;
}

// ============================================================================
// Coverage Gap Tests — Assignment from unexpected
// ============================================================================

FATP_TEST_CASE(unexpected_assignment_nonvoid)
{
    Expected<int, std::string> v(42);
    FATP_ASSERT_TRUE(v.has_value(), "Precondition: value state");

    v = unexpected{"assigned error"};
    FATP_ASSERT_TRUE(!v.has_value(), "Assignment from unexpected sets error state");
    FATP_ASSERT_EQ(v.error(), std::string("assigned error"), "Error payload correct");

    v = unexpected{"second error"};
    FATP_ASSERT_TRUE(!v.has_value(), "Error->error assignment works");
    FATP_ASSERT_EQ(v.error(), std::string("second error"), "Second error correct");
    return true;
}

FATP_TEST_CASE(unexpected_assignment_void)
{
    Expected<void, std::string> v;
    FATP_ASSERT_TRUE(v.has_value(), "Precondition: value state");

    v = unexpected{"void error"};
    FATP_ASSERT_TRUE(!v.has_value(), "Void assignment from unexpected sets error");
    FATP_ASSERT_EQ(v.error(), std::string("void error"), "Void error correct");
    return true;
}

// ============================================================================
// Coverage Gap Tests — Hash
// ============================================================================

FATP_TEST_CASE(hash_error_state)
{
    std::hash<Expected<int, std::string>> hasher;

    Expected<int, std::string> v(42);
    Expected<int, std::string> e(unexpected{"err"});

    size_t vh = hasher(v);
    size_t eh = hasher(e);
    FATP_ASSERT_TRUE(vh != eh, "Value hash and error hash should differ");

    Expected<int, std::string> e2(unexpected{"err"});
    FATP_ASSERT_TRUE(hasher(e) == hasher(e2), "Same errors should hash equal");
    return true;
}

FATP_TEST_CASE(hash_void_expected)
{
    std::hash<Expected<void, std::string>> hasher;

    Expected<void, std::string> v;
    Expected<void, std::string> e(unexpected{"err"});

    size_t vh = hasher(v);
    size_t eh = hasher(e);
    FATP_ASSERT_TRUE(vh != eh, "Void value and error hash should differ");

    Expected<void, std::string> v2;
    FATP_ASSERT_TRUE(hasher(v) == hasher(v2), "Two void values hash equal");
    return true;
}

FATP_TEST_CASE(hash_unexpected)
{
    std::hash<unexpected<int>> hasher;

    unexpected<int> a(42);
    unexpected<int> b(42);
    unexpected<int> c(99);

    FATP_ASSERT_TRUE(hasher(a) == hasher(b), "Same unexpected values hash equal");
    FATP_ASSERT_TRUE(hasher(a) != hasher(c), "Different unexpected values hash differently");
    return true;
}

// ============================================================================
// Coverage Gap Tests — Ordering
// ============================================================================

FATP_TEST_CASE(ordering_error_vs_value)
{
    Expected<int, std::string> v(42);
    Expected<int, std::string> e(unexpected{"err"});

    FATP_ASSERT_TRUE(e < v, "Error should be less than value");
    FATP_ASSERT_TRUE(e <= v, "Error should be <= value");
    FATP_ASSERT_TRUE(v > e, "Value should be greater than error");
    FATP_ASSERT_TRUE(v >= e, "Value should be >= error");
    FATP_ASSERT_TRUE(!(v < e), "Value should not be less than error");
    FATP_ASSERT_TRUE(!(e > v), "Error should not be greater than value");
    return true;
}

FATP_TEST_CASE(ordering_error_vs_error)
{
    Expected<int, std::string> e1(unexpected{"alpha"});
    Expected<int, std::string> e2(unexpected{"beta"});
    Expected<int, std::string> e3(unexpected{"alpha"});

    FATP_ASSERT_TRUE(e1 < e2, "alpha < beta");
    FATP_ASSERT_TRUE(e2 > e1, "beta > alpha");
    FATP_ASSERT_TRUE(e1 <= e3, "alpha <= alpha");
    FATP_ASSERT_TRUE(e1 >= e3, "alpha >= alpha");
    return true;
}

// ============================================================================
// Coverage Gap Tests — Swap
// ============================================================================

FATP_TEST_CASE(swap_error_error)
{
    Expected<int, std::string> e1(unexpected{"one"});
    Expected<int, std::string> e2(unexpected{"two"});

    e1.swap(e2);
    FATP_ASSERT_EQ(e1.error(), std::string("two"), "Error-error swap e1");
    FATP_ASSERT_EQ(e2.error(), std::string("one"), "Error-error swap e2");
    return true;
}

FATP_TEST_CASE(swap_free_function)
{
    Expected<int, std::string> a(10);
    Expected<int, std::string> b(20);

    swap(a, b);
    FATP_ASSERT_EQ(*a, 20, "Free swap a");
    FATP_ASSERT_EQ(*b, 10, "Free swap b");

    Expected<int, std::string> v(42);
    Expected<int, std::string> e(unexpected{"err"});
    swap(v, e);
    FATP_ASSERT_TRUE(!v.has_value(), "Free cross-state swap v");
    FATP_ASSERT_TRUE(e.has_value(), "Free cross-state swap e");
    FATP_ASSERT_EQ(*e, 42, "Free cross-state value preserved");
    return true;
}

// ============================================================================
// Coverage Gap Tests — Inspect Chaining
// ============================================================================

FATP_TEST_CASE(inspect_chaining)
{
    int observed_value = 0;

    auto result = Expected<int, std::string>(7)
                      .inspect([&](int x) { observed_value = x; })
                      .map([](int x) { return x * 3; });

    FATP_ASSERT_EQ(observed_value, 7, "inspect should observe before map");
    FATP_ASSERT_TRUE(result.has_value(), "Chained map should succeed");
    FATP_ASSERT_EQ(*result, 21, "Chained map result correct");

    int error_calls = 0;
    auto result2 = Expected<int, std::string>(5)
                       .inspect_error([&](const std::string&) { ++error_calls; })
                       .map([](int x) { return x + 1; });

    FATP_ASSERT_EQ(error_calls, 0, "inspect_error should not fire on value");
    FATP_ASSERT_EQ(*result2, 6, "Chain after inspect_error works");
    return true;
}

// ============================================================================
// Coverage Gap Tests — Emplace Value-to-Value
// ============================================================================

FATP_TEST_CASE(emplace_value_to_value)
{
    Expected<std::string, int> exp("first");
    FATP_ASSERT_EQ(*exp, std::string("first"), "Precondition");

    auto& ref = exp.emplace("second");
    FATP_ASSERT_EQ(*exp, std::string("second"), "Re-emplace should replace value");
    FATP_ASSERT_EQ(ref, std::string("second"), "emplace returns reference to emplaced");

    ref = "mutated";
    FATP_ASSERT_EQ(*exp, std::string("mutated"), "Mutating returned ref changes stored value");
    return true;
}

// ============================================================================
// Benchmarks
// ============================================================================
// ============================================================================
// Main Test Runner
// ============================================================================

} // namespace fat_p::testing::expected

namespace fat_p::testing
{


void run_benchmarks()
{
    using namespace fat_p::testing;

    auto& out = *get_test_config().output;
    out << "\n" << colors::cyan() << "Sanity Benchmark:" << colors::reset() << "\n";
    out << "  (benchmarks are provided by benchmark executables; unit tests do not run full benchmarks)\n\n";
}

bool test_Expected()
{
    FATP_PRINT_HEADER(EXPECTED)

    TestRunner runner;

    FATP_RUN_TEST_NS(runner, expected, basic_construction);
    FATP_RUN_TEST_NS(runner, expected, copy_construction);
    FATP_RUN_TEST_NS(runner, expected, move_construction);
    FATP_RUN_TEST_NS(runner, expected, copy_assignment);
    FATP_RUN_TEST_NS(runner, expected, move_assignment);
    FATP_RUN_TEST_NS(runner, expected, value_access);
    FATP_RUN_TEST_NS(runner, expected, error_access);
    FATP_RUN_TEST_NS(runner, expected, has_error);
    FATP_RUN_TEST_NS(runner, expected, error_or_else);
    FATP_RUN_TEST_NS(runner, expected, map);
    FATP_RUN_TEST_NS(runner, expected, and_then);
    FATP_RUN_TEST_NS(runner, expected, or_else);
    FATP_RUN_TEST_NS(runner, expected, transform_error);
    FATP_RUN_TEST_NS(runner, expected, inspect);
    FATP_RUN_TEST_NS(runner, expected, void_specialization);
    FATP_RUN_TEST_NS(runner, expected, emplace);
    FATP_RUN_TEST_NS(runner, expected, swap);
    FATP_RUN_TEST_NS(runner, expected, comparisons);
    FATP_RUN_TEST_NS(runner, expected, ordering);
    FATP_RUN_TEST_NS(runner, expected, hash);
    FATP_RUN_TEST_NS(runner, expected, make_expected);
    FATP_RUN_TEST_NS(runner, expected, result_status_aliases);
    FATP_RUN_TEST_NS(runner, expected, storage_policy);
    FATP_RUN_TEST_NS(runner, expected, rebind);
    FATP_RUN_TEST_NS(runner, expected, non_default_constructible);
    FATP_RUN_TEST_NS(runner, expected, large_objects);
    FATP_RUN_TEST_NS(runner, expected, concurrent_read);
    FATP_RUN_TEST_NS(runner, expected, monadic_chaining);
    FATP_RUN_TEST_NS(runner, expected, fold);
    FATP_RUN_TEST_NS(runner, expected, value_unchecked);
    FATP_RUN_TEST_NS(runner, expected, trivial_storage);
    FATP_RUN_TEST_NS(runner, expected, assign_or_return);

#if defined(__cpp_lib_three_way_comparison) && __cpp_lib_three_way_comparison >= 201907L
    FATP_RUN_TEST_NS(runner, expected, map_to_void);
    FATP_RUN_TEST_NS(runner, expected, void_unexpect_default_error);
    FATP_RUN_TEST_NS(runner, expected, converting_constructors);
    FATP_RUN_TEST_NS(runner, expected, trivial_default_and_void_map);
    FATP_RUN_TEST_NS(runner, expected, three_way_comparison);
#endif

#if defined(__cpp_lib_expected) && __cpp_lib_expected >= 202202L
    FATP_RUN_TEST_NS(runner, expected, std_expected_integration);
#endif

    // Coverage gap tests
    FATP_RUN_TEST_NS(runner, expected, bad_expected_access_from_value);
    FATP_RUN_TEST_NS(runner, expected, bad_expected_access_rvalue_error);
    FATP_RUN_TEST_NS(runner, expected, bad_expected_access_void);
    FATP_RUN_TEST_NS(runner, expected, expected_try_success);
    FATP_RUN_TEST_NS(runner, expected, expected_try_propagates_error);
    FATP_RUN_TEST_NS(runner, expected, expected_try_void_success);
    FATP_RUN_TEST_NS(runner, expected, expected_try_void_propagates_error);
    FATP_RUN_TEST_NS(runner, expected, value_or_else_value_path);
    FATP_RUN_TEST_NS(runner, expected, value_or_else_error_path);
    FATP_RUN_TEST_NS(runner, expected, value_or_else_rvalue);
    FATP_RUN_TEST_NS(runner, expected, void_emplace);
    FATP_RUN_TEST_NS(runner, expected, void_swap);
    FATP_RUN_TEST_NS(runner, expected, void_copy_assignment);
    FATP_RUN_TEST_NS(runner, expected, void_move_assignment);
    FATP_RUN_TEST_NS(runner, expected, void_and_then);
    FATP_RUN_TEST_NS(runner, expected, void_or_else);
    FATP_RUN_TEST_NS(runner, expected, void_map_error);
    FATP_RUN_TEST_NS(runner, expected, void_inspect);
    FATP_RUN_TEST_NS(runner, expected, void_fold);
    FATP_RUN_TEST_NS(runner, expected, void_error_or_else);
    FATP_RUN_TEST_NS(runner, expected, void_comparison);
    FATP_RUN_TEST_NS(runner, expected, rvalue_value_access);
    FATP_RUN_TEST_NS(runner, expected, rvalue_error_access);
    FATP_RUN_TEST_NS(runner, expected, rvalue_move_semantics);
    FATP_RUN_TEST_NS(runner, expected, explicit_bool_conversion);
    FATP_RUN_TEST_NS(runner, expected, transform_alias);
    FATP_RUN_TEST_NS(runner, expected, flat_map_alias);
    FATP_RUN_TEST_NS(runner, expected, map_error_direct);
    FATP_RUN_TEST_NS(runner, expected, unexpected_standalone_ops);
    FATP_RUN_TEST_NS(runner, expected, make_unexpected_factory);
    FATP_RUN_TEST_NS(runner, expected, ctad_deduction);
    FATP_RUN_TEST_NS(runner, expected, initializer_list_value_construction);
    FATP_RUN_TEST_NS(runner, expected, initializer_list_error_construction);
    FATP_RUN_TEST_NS(runner, expected, initializer_list_emplace);
    FATP_RUN_TEST_NS(runner, expected, unexpected_assignment_nonvoid);
    FATP_RUN_TEST_NS(runner, expected, unexpected_assignment_void);
    FATP_RUN_TEST_NS(runner, expected, hash_error_state);
    FATP_RUN_TEST_NS(runner, expected, hash_void_expected);
    FATP_RUN_TEST_NS(runner, expected, hash_unexpected);
    FATP_RUN_TEST_NS(runner, expected, ordering_error_vs_value);
    FATP_RUN_TEST_NS(runner, expected, ordering_error_vs_error);
    FATP_RUN_TEST_NS(runner, expected, swap_error_error);
    FATP_RUN_TEST_NS(runner, expected, swap_free_function);
    FATP_RUN_TEST_NS(runner, expected, inspect_chaining);
    FATP_RUN_TEST_NS(runner, expected, emplace_value_to_value);

    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

// =============================================================================
// Standalone Entry Point
// =============================================================================
#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_Expected() ? 0 : 1;
}
#endif
