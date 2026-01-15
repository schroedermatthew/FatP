/**
 * @file test_Expected.cpp
 * @brief Comprehensive unit tests for Expected.h
 */
/*
FATP_META:
  meta_version: 1
  component: Expected
  file_role: test
  path: tests/test_Expected.cpp
  namespace: fat_p::testing::expected
  summary: "Unit tests for Expected."
  related:
    docs_search: "Expected"
    headers:
      - fat_p/Expected.h
      - fat_p/FatPTest.h
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
#include <iostream>
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
    v3 = v3;
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
// Benchmarks
// ============================================================================

void benchmark_expected()
{
    constexpr size_t N = 1000000;

    std::cout << "\n" << colors::yellow() << "Expected Benchmarks" << colors::reset() << "\n\n";

    std::cout << colors::yellow() << "1. Construction" << colors::reset() << "\n";
    benchmark(
        "Value construction",
        []() {
            Expected<int, std::string> e(42);
            DoNotOptimize(e.has_value());
        },
        N);

    benchmark(
        "Error construction",
        []() {
            Expected<int, std::string> e(unexpected{"error"});
            DoNotOptimize(e.has_value());
        },
        N);

    std::cout << "\n" << colors::yellow() << "2. Assignment" << colors::reset() << "\n";
    Expected<int, std::string> assign_target(0);
    Expected<int, std::string> assign_source(42);
    DoNotOptimize(assign_target.has_value());
    DoNotOptimize(assign_source.has_value());

    benchmark(
        "Same-state assignment",
        [&]() {
            assign_target = assign_source;
            DoNotOptimize(assign_target.has_value());
        },
        N);

    Expected<int, std::string> val_exp(42);
    Expected<int, std::string> err_exp(unexpected{"error"});
    DoNotOptimize(val_exp.has_value());
    DoNotOptimize(err_exp.has_value());

    benchmark(
        "Different-state assignment",
        [&]() {
            val_exp = err_exp;
            err_exp = Expected<int, std::string>(100);
            DoNotOptimize(val_exp.has_value());
        },
        N / 10);

    std::cout << "\n" << colors::yellow() << "3. Value Access" << colors::reset() << "\n";
    Expected<int, std::string> access_exp(42);
    DoNotOptimize(access_exp.has_value());

    benchmark(
        "has_value()",
        [&]() {
            bool has = access_exp.has_value();
            DoNotOptimize(has);
        },
        N);

    benchmark(
        "value_or()",
        [&]() {
            int val = access_exp.value_or(0);
            DoNotOptimize(val);
        },
        N);

    benchmark(
        "operator*",
        [&]() {
            int val = *access_exp;
            DoNotOptimize(val);
        },
        N);

    std::cout << "\n" << colors::yellow() << "4. Monadic Operations" << colors::reset() << "\n";
    Expected<int, std::string> monadic_exp(42);
    DoNotOptimize(monadic_exp.has_value());

    benchmark(
        "map()",
        [&]() {
            auto result = monadic_exp.map([](int x) {
                return x * 2;
            });
            DoNotOptimize(result.has_value());
        },
        N);

    benchmark(
        "and_then()",
        [&]() {
            auto result = monadic_exp.and_then([](int x) -> Expected<int, std::string> {
                return x * 2;
            });
            DoNotOptimize(result.has_value());
        },
        N);

    benchmark(
        "or_else() with value",
        [&]() {
            auto result = monadic_exp.or_else([](const std::string&) -> Expected<int, std::string> {
                return 0;
            });
            DoNotOptimize(result.has_value());
        },
        N);

    Expected<int, std::string> err_for_or_else(unexpected{"error"});
    DoNotOptimize(err_for_or_else.has_value());

    benchmark(
        "or_else() with error",
        [&]() {
            auto result = err_for_or_else.or_else([](const std::string&) -> Expected<int, std::string> {
                return 0;
            });
            DoNotOptimize(result.has_value());
        },
        N);

    std::cout << "\n" << colors::yellow() << "5. Comparison with std::optional" << colors::reset() << "\n";
    benchmark(
        "Expected<int> construction",
        []() {
            Expected<int, std::string> e(42);
            DoNotOptimize(e.has_value());
        },
        N);

    benchmark(
        "std::optional<int> construction",
        []() {
            std::optional<int> o(42);
            DoNotOptimize(o.has_value());
        },
        N);

    std::cout << "\n" << colors::yellow() << "6. Void Specialization" << colors::reset() << "\n";
    benchmark(
        "Expected<void> success construction",
        []() {
            Expected<void, std::string> e;
            DoNotOptimize(e.has_value());
        },
        N);

    benchmark(
        "Expected<void> error construction",
        []() {
            Expected<void, std::string> e(unexpected{"error"});
            DoNotOptimize(e.has_value());
        },
        N);

    Expected<void, std::string> void_exp;
    DoNotOptimize(void_exp.has_value());

    benchmark(
        "Expected<void> has_value()",
        [&]() {
            bool has = void_exp.has_value();
            DoNotOptimize(has);
        },
        N);

    benchmark(
        "Expected<void> map()",
        [&]() {
            auto result = void_exp.map([]() {
                return 42;
            });
            DoNotOptimize(result.has_value());
        },
        N);

    benchmark(
        "Status alias construction",
        []() {
            Status s;
            DoNotOptimize(s.has_value());
        },
        N);
}

// ============================================================================
// Main Test Runner
// ============================================================================

} // namespace fat_p::testing::expected

namespace fat_p::testing
{

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
    FATP_RUN_TEST_NS(runner, expected, three_way_comparison);
#endif

#if defined(__cpp_lib_expected) && __cpp_lib_expected >= 202202L
    FATP_RUN_TEST_NS(runner, expected, std_expected_integration);
#endif

    expected::benchmark_expected();

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
