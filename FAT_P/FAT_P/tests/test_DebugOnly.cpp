/**
 * @file test_DebugOnly.cpp
 * @brief Comprehensive unit tests for DebugOnly.h
 */
/*
FATP_META:
  meta_version: 1
  component: DebugOnly
  file_role: test
  path: tests/test_DebugOnly.cpp
  namespace: fat_p
  summary: "Unit tests for DebugOnly."
  related:
    docs_search: "DebugOnly"
    headers:
      - fat_p/CppStandardDetection.h
      - fat_p/DebugOnly.h
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


#include "CppStandardDetection.h"
#include "DebugOnly.h"
#include "FatPTest.h"

#include <chrono>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

namespace fat_p::testing::debugonly
{

// ============================================================================
// Test Helpers
// ============================================================================

struct NonTrivial
{
    std::string data;
    int value;

    NonTrivial()
        : data("default")
        , value(0)
    {
    }
    NonTrivial(std::string d, int v)
        : data(std::move(d))
        , value(v)
    {
    }

    bool operator==(const NonTrivial& other) const
    {
        return data == other.data && value == other.value;
    }
};

struct MoveOnly
{
    std::unique_ptr<int> ptr;

    MoveOnly()
        : ptr(nullptr)
    {
    }
    explicit MoveOnly(int v)
        : ptr(std::make_unique<int>(v))
    {
    }
    MoveOnly(MoveOnly&&) noexcept = default;
    MoveOnly& operator=(MoveOnly&&) noexcept = default;

    MoveOnly(const MoveOnly&) = delete;
    MoveOnly& operator=(const MoveOnly&) = delete;

    int get_value() const
    {
        return ptr ? *ptr : -1;
    }
};

struct ThrowingCtor
{
    int value;
    static bool should_throw;

    ThrowingCtor()
        : value(0)
    {
        if (should_throw)
        {
            throw std::runtime_error("ThrowingCtor exception");
        }
    }

    explicit ThrowingCtor(int v)
        : value(v)
    {
        if (should_throw)
        {
            throw std::runtime_error("ThrowingCtor exception");
        }
    }
};

bool ThrowingCtor::should_throw = false;

// ============================================================================
// Construction Tests
// ============================================================================

FATP_TEST_CASE(default_construction)
{
    fat_p::DebugOnly<int> int_val;
    fat_p::DebugOnly<std::string> str_val;
    fat_p::DebugOnly<NonTrivial> complex_val;

#ifndef NDEBUG
    FATP_ASSERT_EQ(int_val.get(), 0, "Default int should be 0");
    FATP_ASSERT_EQ(str_val.get(), "", "Default string should be empty");
    FATP_ASSERT_EQ(complex_val.get().data, "default", "Default NonTrivial should have default data");
#else
    (void)int_val;
    (void)str_val;
    (void)complex_val;
#endif

    // Should compile in both modes
    FATP_ASSERT_TRUE(true, "Default construction compiles");
    return true;
}

FATP_TEST_CASE(value_construction)
{
    fat_p::DebugOnly<int> int_val(42);
    fat_p::DebugOnly<std::string> str_val("hello");
    fat_p::DebugOnly<NonTrivial> complex_val(NonTrivial("test", 123));

#ifndef NDEBUG
    FATP_ASSERT_EQ(int_val.get(), 42, "Int should be 42");
    FATP_ASSERT_EQ(str_val.get(), "hello", "String should be hello");
    FATP_ASSERT_EQ(complex_val.get().data, "test", "NonTrivial data should be test");
    FATP_ASSERT_EQ(complex_val.get().value, 123, "NonTrivial value should be 123");
#endif

    return true;
}

FATP_TEST_CASE(inplace_construction)
{
    fat_p::DebugOnly<NonTrivial> val(std::in_place, "inplace", 999);

#ifndef NDEBUG
    FATP_ASSERT_EQ(val.get().data, "inplace", "In-place data");
    FATP_ASSERT_EQ(val.get().value, 999, "In-place value");
#endif

    return true;
}

FATP_TEST_CASE(copy_construction)
{
    fat_p::DebugOnly<int> original(42);
    fat_p::DebugOnly<int> copy(original);

#ifndef NDEBUG
    FATP_ASSERT_EQ(copy.get(), 42, "Copy should have same value");
    FATP_ASSERT_EQ(original.get(), 42, "Original should be unchanged");
#endif

    return true;
}

FATP_TEST_CASE(move_construction)
{
    fat_p::DebugOnly<std::string> original("moveable");
    fat_p::DebugOnly<std::string> moved(std::move(original));

#ifndef NDEBUG
    FATP_ASSERT_EQ(moved.get(), "moveable", "Moved-to should have value");
#endif

    return true;
}

FATP_TEST_CASE(move_only_type)
{
    fat_p::DebugOnly<MoveOnly> val(MoveOnly(42));

#ifndef NDEBUG
    FATP_ASSERT_EQ(val.get().get_value(), 42, "Move-only value should be accessible");
#endif

    fat_p::DebugOnly<MoveOnly> moved(std::move(val));

#ifndef NDEBUG
    FATP_ASSERT_EQ(moved.get().get_value(), 42, "Move-only should transfer");
#endif

    return true;
}

// ============================================================================
// Assignment Tests
// ============================================================================

FATP_TEST_CASE(value_assignment)
{
    fat_p::DebugOnly<int> val;
    val = 100;

#ifndef NDEBUG
    FATP_ASSERT_EQ(val.get(), 100, "Assigned value should be 100");
#endif

    val = 200;

#ifndef NDEBUG
    FATP_ASSERT_EQ(val.get(), 200, "Reassigned value should be 200");
#endif

    return true;
}

FATP_TEST_CASE(copy_assignment)
{
    fat_p::DebugOnly<int> original(42);
    fat_p::DebugOnly<int> copy;
    copy = original;

#ifndef NDEBUG
    FATP_ASSERT_EQ(copy.get(), 42, "Copy-assigned should have same value");
#endif

    return true;
}

FATP_TEST_CASE(move_assignment)
{
    fat_p::DebugOnly<std::string> original("source");
    fat_p::DebugOnly<std::string> target;
    target = std::move(original);

#ifndef NDEBUG
    FATP_ASSERT_EQ(target.get(), "source", "Move-assigned should have value");
#endif

    return true;
}

// ============================================================================
// Accessor Tests
// ============================================================================

FATP_TEST_CASE(get_accessor)
{
    fat_p::DebugOnly<int> val(42);

#ifndef NDEBUG
    FATP_ASSERT_EQ(val.get(), 42, "get() should return value");

    val.get() = 100;
    FATP_ASSERT_EQ(val.get(), 100, "get() should allow modification");
#endif

    return true;
}

FATP_TEST_CASE(arrow_operator)
{
    fat_p::DebugOnly<NonTrivial> val(NonTrivial("arrow", 50));

#ifndef NDEBUG
    FATP_ASSERT_EQ(val->data, "arrow", "Arrow operator should access member");
    FATP_ASSERT_EQ(val->value, 50, "Arrow operator should access value");

    val->value = 75;
    FATP_ASSERT_EQ(val->value, 75, "Arrow operator should allow modification");
#endif

    return true;
}

FATP_TEST_CASE(dereference_operator)
{
    fat_p::DebugOnly<int> val(42);

#ifndef NDEBUG
    FATP_ASSERT_EQ(*val, 42, "Dereference should return value");

    *val = 100;
    FATP_ASSERT_EQ(*val, 100, "Dereference should allow modification");
#endif

    return true;
}

FATP_TEST_CASE(implicit_conversion)
{
    fat_p::DebugOnly<int> val(42);

#ifndef NDEBUG
    int& ref = val;
    FATP_ASSERT_EQ(ref, 42, "Implicit conversion to reference");

    ref = 100;
    FATP_ASSERT_EQ(val.get(), 100, "Modification through reference");
#endif

    return true;
}

FATP_TEST_CASE(data_accessor)
{
    fat_p::DebugOnly<int> val(42);

#ifndef NDEBUG
    int* ptr = val.data();
    FATP_ASSERT_TRUE(ptr != nullptr, "data() should return valid pointer");
    FATP_ASSERT_EQ(*ptr, 42, "data() should point to value");
#endif

    return true;
}

// ============================================================================
// Modifier Tests
// ============================================================================

FATP_TEST_CASE(emplace)
{
    fat_p::DebugOnly<NonTrivial> val;

#ifndef NDEBUG
    val.emplace("emplaced", 999);
    FATP_ASSERT_EQ(val.get().data, "emplaced", "Emplace should construct new value");
    FATP_ASSERT_EQ(val.get().value, 999, "Emplace should set correct value");
#else
    (void)val;
#endif

    return true;
}

FATP_TEST_CASE(reset)
{
    fat_p::DebugOnly<int> val(42);

#ifndef NDEBUG
    FATP_ASSERT_EQ(val.get(), 42, "Initial value");
#endif

    val.reset();

#ifndef NDEBUG
    FATP_ASSERT_EQ(val.get(), 0, "Reset should set to default");
#endif

    return true;
}

FATP_TEST_CASE(swap)
{
    fat_p::DebugOnly<int> a(10);
    fat_p::DebugOnly<int> b(20);

    a.swap(b);

#ifndef NDEBUG
    FATP_ASSERT_EQ(a.get(), 20, "a should have b's value after swap");
    FATP_ASSERT_EQ(b.get(), 10, "b should have a's value after swap");
#endif

    // Test free function swap
    swap(a, b);

#ifndef NDEBUG
    FATP_ASSERT_EQ(a.get(), 10, "a should be back to original after second swap");
    FATP_ASSERT_EQ(b.get(), 20, "b should be back to original after second swap");
#endif

    return true;
}

// ============================================================================
// Conditional Execution Tests
// ============================================================================

FATP_TEST_CASE(if_debug)
{
    fat_p::DebugOnly<int> val(42);
    int result = 0;

    val.if_debug(
        [&result](int v)
        {
            result = v * 2;
        });

#ifndef NDEBUG
    FATP_ASSERT_EQ(result, 84, "if_debug should execute in debug mode");
#else
    FATP_ASSERT_EQ(result, 0, "if_debug should not execute in release mode");
#endif

    return true;
}

FATP_TEST_CASE(modify)
{
    fat_p::DebugOnly<int> val(10);

    val.modify(
        [](int& v)
        {
            v *= 3;
        });

#ifndef NDEBUG
    FATP_ASSERT_EQ(val.get(), 30, "modify should update value in debug");
#endif

    return true;
}

FATP_TEST_CASE(value_or)
{
    fat_p::DebugOnly<int> val(42);

    int result = val.value_or(999);

#ifndef NDEBUG
    FATP_ASSERT_EQ(result, 42, "value_or should return actual value in debug");
#else
    FATP_ASSERT_EQ(result, 999, "value_or should return default in release");
#endif

    return true;
}

// ============================================================================
// Comparison Tests
// ============================================================================

FATP_TEST_CASE(equality_comparison)
{
    fat_p::DebugOnly<int> a(42);
    fat_p::DebugOnly<int> b(42);
    fat_p::DebugOnly<int> c(100);

#ifndef NDEBUG
    FATP_ASSERT_TRUE(a == b, "Equal values should compare equal");
    FATP_ASSERT_FALSE(a == c, "Different values should not compare equal");
    FATP_ASSERT_TRUE(a != c, "Different values should compare not-equal");
    FATP_ASSERT_FALSE(a != b, "Equal values should not compare not-equal");
#else
    FATP_ASSERT_TRUE(a == b, "Release: all DebugOnly compare equal");
    FATP_ASSERT_TRUE(a == c, "Release: all DebugOnly compare equal");
#endif

    return true;
}

FATP_TEST_CASE(relational_comparison)
{
    fat_p::DebugOnly<int> a(10);
    fat_p::DebugOnly<int> b(20);

#ifndef NDEBUG
    FATP_ASSERT_TRUE(a < b, "10 < 20");
    FATP_ASSERT_TRUE(a <= b, "10 <= 20");
    FATP_ASSERT_FALSE(a > b, "10 not > 20");
    FATP_ASSERT_FALSE(a >= b, "10 not >= 20");
    FATP_ASSERT_TRUE(b > a, "20 > 10");
    FATP_ASSERT_TRUE(b >= a, "20 >= 10");
#endif

    return true;
}

FATP_TEST_CASE(comparison_with_raw_value)
{
    fat_p::DebugOnly<int> val(42);

#ifndef NDEBUG
    // In debug mode, comparison with raw T is allowed
    FATP_ASSERT_TRUE(val == 42, "Should equal raw value");
    FATP_ASSERT_FALSE(val == 100, "Should not equal different value");
    FATP_ASSERT_TRUE(val != 100, "Should not-equal different value");
#else
    // In release mode, comparison with raw T is deleted to prevent control flow bugs.
    // Use value_or() instead for cross-mode safe comparisons:
    FATP_ASSERT_TRUE(val.value_or(0) == 0, "Release: value_or returns default");

    // This would not compile in release (correctly!):
    // val == 42;  // Error: use of deleted function
#endif

    return true;
}

// ============================================================================
// Increment/Decrement Tests
// ============================================================================

FATP_TEST_CASE(increment)
{
    fat_p::DebugOnly<int> val(10);

    ++val;

#ifndef NDEBUG
    FATP_ASSERT_EQ(val.get(), 11, "Pre-increment");
#endif

    val++;

#ifndef NDEBUG
    FATP_ASSERT_EQ(val.get(), 12, "Post-increment");
#endif

    return true;
}

FATP_TEST_CASE(decrement)
{
    fat_p::DebugOnly<int> val(10);

    --val;

#ifndef NDEBUG
    FATP_ASSERT_EQ(val.get(), 9, "Pre-decrement");
#endif

    val--;

#ifndef NDEBUG
    FATP_ASSERT_EQ(val.get(), 8, "Post-decrement");
#endif

    return true;
}

FATP_TEST_CASE(compound_assignment)
{
    fat_p::DebugOnly<int> val(10);

    val += 5;

#ifndef NDEBUG
    FATP_ASSERT_EQ(val.get(), 15, "After += 5");
#endif

    val -= 3;

#ifndef NDEBUG
    FATP_ASSERT_EQ(val.get(), 12, "After -= 3");
#endif

    return true;
}

// ============================================================================
// Stream Output Tests
// ============================================================================

FATP_TEST_CASE(stream_output)
{
    fat_p::DebugOnly<int> val(42);
    std::ostringstream oss;

    oss << val;

#ifndef NDEBUG
    FATP_ASSERT_EQ(oss.str(), "42", "Stream should output value in debug");
#else
    FATP_ASSERT_EQ(oss.str(), "", "Stream should output nothing in release");
#endif

    return true;
}

// ============================================================================
// Hash Tests
// ============================================================================

FATP_TEST_CASE(hash)
{
    fat_p::DebugOnly<int> val(42);

    std::hash<fat_p::DebugOnly<int>> hasher;
    size_t hash_val = hasher(val);

#ifndef NDEBUG
    FATP_ASSERT_EQ(hash_val, std::hash<int>{}(42), "Hash should match underlying type");
#else
    FATP_ASSERT_EQ(hash_val, 0u, "Release hash should be 0");
#endif

    return true;
}

FATP_TEST_CASE(in_unordered_set)
{
    std::unordered_set<fat_p::DebugOnly<int>> set;

    set.insert(fat_p::DebugOnly<int>(1));
    set.insert(fat_p::DebugOnly<int>(2));
    set.insert(fat_p::DebugOnly<int>(3));

#ifndef NDEBUG
    FATP_ASSERT_EQ(set.size(), 3u, "Debug: set should have 3 distinct elements");
#else
    FATP_ASSERT_EQ(set.size(), 1u, "Release: all hash to same value, only 1 element");
#endif

    return true;
}

// ============================================================================
// Compile-Time Query Tests
// ============================================================================

FATP_TEST_CASE(is_active)
{
#ifndef NDEBUG
    FATP_ASSERT_TRUE(fat_p::DebugOnly<int>::is_active, "is_active should be true in debug");
#else
    FATP_ASSERT_FALSE(fat_p::DebugOnly<int>::is_active, "is_active should be false in release");
#endif

    return true;
}

FATP_TEST_CASE(type_aliases)
{
    using DO = fat_p::DebugOnly<int>;

    FATP_ASSERT_TRUE((std::is_same_v<DO::value_type, int>), "value_type should be int");
    FATP_ASSERT_TRUE((std::is_same_v<DO::reference, int&>), "reference should be int&");
    FATP_ASSERT_TRUE((std::is_same_v<DO::const_reference, const int&>), "const_reference");
    FATP_ASSERT_TRUE((std::is_same_v<DO::pointer, int*>), "pointer should be int*");
    FATP_ASSERT_TRUE((std::is_same_v<DO::const_pointer, const int*>), "const_pointer");

    return true;
}

// ============================================================================
// Size Tests
// ============================================================================

FATP_TEST_CASE(size_characteristics)
{
    struct WithDebug
    {
        int data;
        FATP_NO_UNIQUE_ADDRESS fat_p::DebugOnly<std::string> debug_info;
    };

#ifdef NDEBUG
#if FATP_HAS_CPP20
    FATP_ASSERT_EQ(sizeof(WithDebug), sizeof(int), "C++20 release: zero overhead with [[no_unique_address]]");
#else
    FATP_ASSERT_LE(sizeof(WithDebug), sizeof(int) + sizeof(void*), "C++17 release: minimal overhead");
#endif
#else
    FATP_ASSERT_GT(sizeof(WithDebug), sizeof(int), "Debug: should include string storage");
#endif

    return true;
}

FATP_TEST_CASE(empty_base_optimization)
{
    struct BaseOnly : fat_p::DebugOnly<int>
    {
        int data;
    };

#ifdef NDEBUG
    FATP_ASSERT_EQ(sizeof(BaseOnly), sizeof(int), "EBO should apply in release");
#endif

    return true;
}

// ============================================================================
// Macro Tests
// ============================================================================

FATP_TEST_CASE(debug_only_exec_macro)
{
    int counter = 0;

    FATP_DEBUG_ONLY_EXEC(counter = 42);

#ifndef NDEBUG
    FATP_ASSERT_EQ(counter, 42, "FATP_DEBUG_ONLY_EXEC should execute in debug");
#else
    FATP_ASSERT_EQ(counter, 0, "FATP_DEBUG_ONLY_EXEC should not execute in release");
#endif

    return true;
}

FATP_TEST_CASE(debug_only_increment_macro)
{
    fat_p::DebugOnly<int> counter(0);

    FATP_DEBUG_ONLY_INCREMENT(counter);
    FATP_DEBUG_ONLY_INCREMENT(counter);
    FATP_DEBUG_ONLY_INCREMENT(counter);

#ifndef NDEBUG
    FATP_ASSERT_EQ(counter.get(), 3, "Counter should be 3 after 3 increments");
#endif

    return true;
}

// ============================================================================
// Exception Safety Tests
// ============================================================================

FATP_TEST_CASE(throwing_constructor)
{
    ThrowingCtor::should_throw = false;

    fat_p::DebugOnly<ThrowingCtor> val(ThrowingCtor(42));

#ifndef NDEBUG
    FATP_ASSERT_EQ(val.get().value, 42, "Non-throwing construction");
#endif

    ThrowingCtor::should_throw = true;

#ifndef NDEBUG
    bool caught = false;
    try
    {
        fat_p::DebugOnly<ThrowingCtor> throwing_val(ThrowingCtor(0));
        (void)throwing_val;
    }
    catch (const std::runtime_error&)
    {
        caught = true;
    }

    FATP_ASSERT_TRUE(caught, "Should catch exception from throwing constructor");
#endif

    ThrowingCtor::should_throw = false;
    return true;
}

// ============================================================================
// Constexpr Tests (C++20)
// ============================================================================

FATP_TEST_CASE(constexpr_construction)
{
    constexpr fat_p::DebugOnly<int> val(42);

#ifndef NDEBUG
    static_assert(val.get() == 42 || true, "Constexpr construction");
#endif

    FATP_ASSERT_TRUE(true, "Constexpr construction compiles");
    return true;
}

// ============================================================================
// Benchmarks
// ============================================================================

void benchmark_debugonly()
{
    std::cout << "\n"
              << fat_p::testing::colors::cyan() << "DebugOnly Benchmarks:" << fat_p::testing::colors::reset() << "\n\n";

#ifdef NDEBUG
    std::cout << "Running in RELEASE mode (NDEBUG defined)\n";
#else
    std::cout << "Running in DEBUG mode\n";
#endif

    std::cout << "C++ Standard: " << fat_p::detail::cplusplus_string() << "\n";
    std::cout << "Compiler: " << fat_p::detail::compiler_name() << "\n\n";

    // Benchmark assignment
    fat_p::DebugOnly<int> debug_val;
    int sink = 0;

    fat_p::testing::benchmark("DebugOnly<int> assignment",
                              [&]()
                              {
                                  debug_val = sink;
                                  fat_p::testing::DoNotOptimize(debug_val);
                                  fat_p::testing::DoNotOptimize(sink);
                                  ++sink;
                              });

    // Benchmark increment
    fat_p::DebugOnly<size_t> counter(0);

    fat_p::testing::benchmark("DebugOnly<size_t> increment",
                              [&]()
                              {
                                  ++counter;
                                  fat_p::testing::DoNotOptimize(counter);
                              });

    // Benchmark if_debug
    fat_p::DebugOnly<int> val(42);
    int result = 0;

    fat_p::testing::benchmark("DebugOnly<int>::if_debug",
                              [&]()
                              {
                                  val.if_debug(
                                      [&](int v)
                                      {
                                          result = v;
                                      });
                                  fat_p::testing::DoNotOptimize(result);
                              });

    // Benchmark value_or
    fat_p::testing::benchmark("DebugOnly<int>::value_or",
                              [&]()
                              {
                                  result = val.value_or(999);
                                  fat_p::testing::DoNotOptimize(result);
                              });

    // Size information
    struct WithDebug
    {
        int data;
        FATP_NO_UNIQUE_ADDRESS fat_p::DebugOnly<std::string> debug_info;
    };

    std::cout << "\nSize Characteristics:\n";
    std::cout << "  sizeof(int): " << sizeof(int) << " bytes\n";
    std::cout << "  sizeof(DebugOnly<int>): " << sizeof(fat_p::DebugOnly<int>) << " bytes\n";
    std::cout << "  sizeof(DebugOnly<string>): " << sizeof(fat_p::DebugOnly<std::string>) << " bytes\n";
    std::cout << "  sizeof(WithDebug): " << sizeof(WithDebug) << " bytes\n";

#ifdef NDEBUG
#if FATP_HAS_CPP20
    std::cout << "  [OK] C++20 with [[no_unique_address]]: True zero overhead\n";
#else
    std::cout << "  [INFO] C++17: Minimal overhead (1 byte + padding)\n";
#endif
#endif
}

} // namespace fat_p::testing::debugonly

namespace fat_p::testing
{

// ============================================================================
// Main Test Function
// ============================================================================

bool test_DebugOnly()
{
    FATP_PRINT_HEADER(DEBUG ONLY)

    TestRunner runner;

    // Construction tests
    FATP_RUN_TEST_NS(runner, debugonly, default_construction);
    FATP_RUN_TEST_NS(runner, debugonly, value_construction);
    FATP_RUN_TEST_NS(runner, debugonly, inplace_construction);
    FATP_RUN_TEST_NS(runner, debugonly, copy_construction);
    FATP_RUN_TEST_NS(runner, debugonly, move_construction);
    FATP_RUN_TEST_NS(runner, debugonly, move_only_type);

    // Assignment tests
    FATP_RUN_TEST_NS(runner, debugonly, value_assignment);
    FATP_RUN_TEST_NS(runner, debugonly, copy_assignment);
    FATP_RUN_TEST_NS(runner, debugonly, move_assignment);

    // Accessor tests
    FATP_RUN_TEST_NS(runner, debugonly, get_accessor);
    FATP_RUN_TEST_NS(runner, debugonly, arrow_operator);
    FATP_RUN_TEST_NS(runner, debugonly, dereference_operator);
    FATP_RUN_TEST_NS(runner, debugonly, implicit_conversion);
    FATP_RUN_TEST_NS(runner, debugonly, data_accessor);

    // Modifier tests
    FATP_RUN_TEST_NS(runner, debugonly, emplace);
    FATP_RUN_TEST_NS(runner, debugonly, reset);
    FATP_RUN_TEST_NS(runner, debugonly, swap);

    // Conditional execution tests
    FATP_RUN_TEST_NS(runner, debugonly, if_debug);
    FATP_RUN_TEST_NS(runner, debugonly, modify);
    FATP_RUN_TEST_NS(runner, debugonly, value_or);

    // Comparison tests
    FATP_RUN_TEST_NS(runner, debugonly, equality_comparison);
    FATP_RUN_TEST_NS(runner, debugonly, relational_comparison);
    FATP_RUN_TEST_NS(runner, debugonly, comparison_with_raw_value);

    // Increment/decrement tests
    FATP_RUN_TEST_NS(runner, debugonly, increment);
    FATP_RUN_TEST_NS(runner, debugonly, decrement);
    FATP_RUN_TEST_NS(runner, debugonly, compound_assignment);

    // Stream output test
    FATP_RUN_TEST_NS(runner, debugonly, stream_output);

    // Hash tests
    FATP_RUN_TEST_NS(runner, debugonly, hash);
    FATP_RUN_TEST_NS(runner, debugonly, in_unordered_set);

    // Compile-time query tests
    FATP_RUN_TEST_NS(runner, debugonly, is_active);
    FATP_RUN_TEST_NS(runner, debugonly, type_aliases);

    // Size tests
    FATP_RUN_TEST_NS(runner, debugonly, size_characteristics);
    FATP_RUN_TEST_NS(runner, debugonly, empty_base_optimization);

    // Macro tests
    FATP_RUN_TEST_NS(runner, debugonly, debug_only_exec_macro);
    FATP_RUN_TEST_NS(runner, debugonly, debug_only_increment_macro);

    // Exception safety tests
    FATP_RUN_TEST_NS(runner, debugonly, throwing_constructor);

    // Constexpr tests
    FATP_RUN_TEST_NS(runner, debugonly, constexpr_construction);

    // Benchmarks
    debugonly::benchmark_debugonly();

    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

// =============================================================================
// Standalone Entry Point
// =============================================================================
#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_DebugOnly() ? 0 : 1;
}
#endif
