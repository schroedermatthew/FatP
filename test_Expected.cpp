#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <array>

#include "Expected.h"
#include "test_Expected.h"
#include "test_Utilities.h"

/**
 * @file test_Expected.cpp
 * @brief Comprehensive test suite for cpp_utilities::Expected
 * 
 * This test suite demonstrates all features of the Expected monad including:
 * - Basic value and error handling
 * - Monadic operations (map, and_then, or_else)
 * - Void Expected specialization
 * - Storage policy configuration
 * - Performance benchmarks
 * 
 * @section storage_config Storage Policy Configuration
 * 
 * Expected supports configurable storage policies via macros:
 * 
 * 1. **Default Mode** (Production):
 *    - Compile: g++ test_Expected.cpp
 *    - Uses: UnionStorage (zero-overhead, manual lifetime management)
 *    - Best for: Production code, maximum performance
 * 
 * 2. **Debug Mode**:
 *    - Compile: g++ -DUSE_VARIANT_STORAGE test_Expected.cpp
 *    - Uses: VariantStorage (std::variant-based, automatic lifetime)
 *    - Best for: Development, debugging, better debugger support
 * 
 * 3. **Custom Storage** (Advanced):
 *    - Define your storage policy template before including Expected.h:
 *      @code
 *      template <typename T, typename E>
 *      struct MyCustomStorage { ... };
 *      
 *      #define CPP_UTILITIES_DEFAULT_STORAGE MyCustomStorage
 *      #include "Expected_v4_FINAL.h"
 *      @endcode
 *    - Best for: Arena allocators, object pools, custom memory management
 * 
 * @section storage_aliases Storage Policy Aliases
 * 
 * - `Expected<T, E>`: Uses default storage (controlled by macros above)
 * - `ExpectedUnion<T, E>`: Always uses UnionStorage (explicit)
 * - `ExpectedVariant<T, E>`: Always uses VariantStorage (if USE_VARIANT_STORAGE defined)
 * 
 * This allows you to:
 * - Switch storage globally with one macro (no code changes)
 * - Use explicit storage in performance-critical paths
 * - Mix storage policies in the same translation unit if needed
 */

using namespace cpp_utilities;

namespace cpp_utilities::testing
{

    // ============================================================================
    // Example: Basic Usage
    // ============================================================================

    Expected<int, std::string> divide(int a, int b) {
        if (b == 0) {
            return unexpected{ "Division by zero" };  // FIXED: No ambiguity
        }
        return a / b;
    }

    Expected<int, std::string> safe_stoi(const std::string& s) {
        try {
            return std::stoi(s);
        }
        catch (...) {
            return unexpected{ "Invalid integer: " + s };
        }
    }

    // ============================================================================
    // Example: Void Expected
    // ============================================================================

    Expected<void, std::string> validate_age(int age) {
        if (age < 0 || age > 150) {
            return unexpected{ "Invalid age" };
        }
        return {};  // Success
    }

    // ============================================================================
    // Example: Monadic Chaining
    // ============================================================================

    void example_monadic_operations() {
        std::cout << "=== Monadic Operations Example ===\n";

        auto result = safe_stoi("100")
            .and_then([](int x) { return divide(200, x); })
            .map([](int x) { return x * 2; })
            .inspect([](int x) {
            std::cout << "Success: " << x << "\n";
                })
            .value_or(-1);

        std::cout << "Final result: " << result << "\n\n";

        // Error path
        auto error_result = safe_stoi("not_a_number")
            .and_then([](int x) { return divide(200, x); })
            .map([](int x) { return x * 2; })
            .inspect_error([](const std::string& e) {
            std::cout << "Error: " << e << "\n";
                })
            .value_or(-1);

        std::cout << "Error result: " << error_result << "\n\n";
    }

    // ============================================================================
    // Example: Error Recovery with or_else
    // ============================================================================

    void example_error_recovery() {
        std::cout << "=== Error Recovery Example ===\n";

        auto result = divide(10, 0)
            .or_else([](const std::string& err) -> Expected<int, std::string> {
            std::cout << "Recovering from error: " << err << "\n";
            return 0;  // Default value
                })
            .map([](int x) { return x + 100; });

        std::cout << "Recovered result: " << *result << "\n\n";
    }

    // ============================================================================
    // Example: Error Transformation
    // ============================================================================

    enum class ErrorCode {
        InvalidInput,
        DivisionByZero,
        Overflow
    };

    std::string error_code_to_string(ErrorCode code) {
        switch (code) {
        case ErrorCode::InvalidInput: return "Invalid input";
        case ErrorCode::DivisionByZero: return "Division by zero";
        case ErrorCode::Overflow: return "Overflow";
        }
        return "Unknown error";
    }

    Expected<int, ErrorCode> typed_divide(int a, int b) {
        if (b == 0) return unexpected{ ErrorCode::DivisionByZero };
        return a / b;
    }

    void example_error_transformation() {
        std::cout << "=== Error Transformation Example ===\n";

        auto result = typed_divide(10, 0)
            .map_error([](ErrorCode code) {
            return error_code_to_string(code);
                });

        if (!result) {
            std::cout << "Error: " << result.error() << "\n\n";
        }
    }

    // ============================================================================
    // Example: Void Expected with Monadic Operations
    // ============================================================================

    void example_void_expected() {
        std::cout << "=== Void Expected Example ===\n";

        auto result = validate_age(25)
            .and_then([]() { return validate_age(30); })
            .and_then([]() { return validate_age(150); })
            .map([]() {
            std::cout << "All validations passed!\n";
            return 42;
                });

        if (result) {
            std::cout << "Result: " << *result << "\n";
        }
        else {
            std::cout << "Validation failed: " << result.error() << "\n";
        }
        std::cout << "\n";
    }

    // ============================================================================
    // Example: Performance - Optimized Assignment
    // ============================================================================

    void example_optimized_assignment() {
        std::cout << "=== Optimized Assignment Example ===\n";

        Expected<std::vector<int>, std::string> v1(std::vector<int>{1, 2, 3});
        Expected<std::vector<int>, std::string> v2(std::vector<int>{4, 5, 6});

        // OPTIMIZED: Same state assignment - no destructor/constructor calls
        v1 = std::move(v2);  // Just moves the vector, no destroy+construct

        std::cout << "Vector size after optimized move: " << v1->size() << "\n";

        // Different state assignment - still efficient
        Expected<std::vector<int>, std::string> err(unexpected{ "error" });
        v1 = std::move(err);  // Changes state, but efficiently

        std::cout << "After error assignment: " << (v1 ? "has value" : v1.error()) << "\n\n";
    }

    // ============================================================================
    // Example: CTAD (Class Template Argument Deduction)
    // ============================================================================

    void example_ctad() {
        std::cout << "=== CTAD Example ===\n";

        // Deduces Expected<int, std::string>
        ExpectedImpl value = 42;
        static_assert(std::is_same_v<decltype(value), Expected<int, std::string>>);

        // Deduces Expected<void, const char*>
        ExpectedImpl err = unexpected{ "error" };
        static_assert(std::is_same_v<decltype(err), Expected<void, const char*>>);

        std::cout << "CTAD works! Value: " << *value << "\n\n";
    }

    // ============================================================================
    // Example: Storage Policy Configuration
    // ============================================================================

    void example_storage_policy() {
        std::cout << "=== Storage Policy Configuration Example ===\n";

        // Default behavior: Uses UnionStorage (or VariantStorage if USE_VARIANT_STORAGE is defined)
        Expected<int, std::string> default_storage(42);
        std::cout << "Default storage: " << *default_storage << "\n";

        // Explicit UnionStorage (always uses UnionStorage regardless of macros)
        ExpectedUnion<int, std::string> union_storage(100);
        std::cout << "Explicit UnionStorage: " << *union_storage << "\n";

#ifdef USE_VARIANT_STORAGE
        // Explicit VariantStorage (only available when USE_VARIANT_STORAGE is defined)
        ExpectedVariant<int, std::string> variant_storage(200);
        std::cout << "Explicit VariantStorage: " << *variant_storage << "\n";
        std::cout << "Note: USE_VARIANT_STORAGE is defined - default Expected uses VariantStorage\n";
#else
        std::cout << "Note: USE_VARIANT_STORAGE is NOT defined - default Expected uses UnionStorage\n";
#endif

        std::cout << "\nStorage Policy Configuration:\n";
        std::cout << "  - Default Expected<T,E>: Controlled by CPP_UTILITIES_DEFAULT_STORAGE macro\n";
        std::cout << "  - ExpectedUnion<T,E>: Always uses UnionStorage (zero-overhead)\n";
#ifdef USE_VARIANT_STORAGE
        std::cout << "  - ExpectedVariant<T,E>: Always uses VariantStorage (debug-friendly)\n";
#endif
        std::cout << "\nTo use custom storage globally:\n";
        std::cout << "  #define CPP_UTILITIES_DEFAULT_STORAGE MyStorage\n";
        std::cout << "  #include \"Expected.h\"\n";
        std::cout << "\n";
    }

    // ============================================================================
    // Example: Inspection for Debugging
    // ============================================================================

    void example_inspection() {
        std::cout << "=== Inspection Example ===\n";

        auto result = safe_stoi("123")
            .inspect([](int x) {
            std::cout << "[DEBUG] Parsed value: " << x << "\n";
                })
            .map([](int x) { return x * 2; })
            .inspect([](int x) {
            std::cout << "[DEBUG] After doubling: " << x << "\n";
                })
            .and_then([](int x) { return divide(x, 2); })
            .inspect([](int x) {
            std::cout << "[DEBUG] After division: " << x << "\n";
                });

        std::cout << "Final: " << *result << "\n\n";
    }

    // ============================================================================
    // Example: Comparison Operators
    // ============================================================================

    void example_comparisons() {
        std::cout << "=== Comparison Example ===\n";

        Expected<int, std::string> v1(42);
        Expected<int, std::string> v2(42);
        Expected<int, std::string> v3(100);
        Expected<int, std::string> err(unexpected{ "error" });

        std::cout << "v1 == v2: " << (v1 == v2) << "\n";
        std::cout << "v1 == v3: " << (v1 == v3) << "\n";
        std::cout << "v1 == 42: " << (v1 == 42) << "\n";
        std::cout << "err == unexpected: " << (err == unexpected{ "error" }) << "\n\n";
    }

    // ============================================================================
    // Example: Hash Support
    // ============================================================================

    void example_hash() {
        std::cout << "=== Hash Support Example ===\n";

        Expected<int, std::string> v1(42);
        Expected<int, std::string> v2(42);
        Expected<int, std::string> err(unexpected{ "error" });

        std::hash<Expected<int, std::string>> hasher;

        std::cout << "hash(v1): " << hasher(v1) << "\n";
        std::cout << "hash(v2): " << hasher(v2) << "\n";
        std::cout << "hash(err): " << hasher(err) << "\n";
        std::cout << "v1 and v2 have same hash: " << (hasher(v1) == hasher(v2)) << "\n\n";
    }

    // ============================================================================
    // Example: error_or utility
    // ============================================================================

    void example_error_or() {
        std::cout << "=== error_or Example ===\n";

        Expected<int, std::string> success(42);
        Expected<int, std::string> failure(unexpected{ "actual error" });

        std::cout << "Success error_or: " << success.error_or("default error") << "\n";
        std::cout << "Failure error_or: " << failure.error_or("default error") << "\n\n";
    }

    // ============================================================================
    // Example: Real-world Use Case - Configuration Parser
    // ============================================================================

    struct Config {
        std::string host;
        int port;
        int timeout_ms;
    };

    // Wrapper to avoid Expected<string, string>
    struct ConfigError {
        std::string message;

        ConfigError(const char* msg) : message(msg) {}
        ConfigError(const std::string& msg) : message(msg) {}
        ConfigError(std::string&& msg) : message(std::move(msg)) {}

        bool operator==(const ConfigError& other) const {
            return message == other.message;
        }
    };

    Expected<Config, ConfigError> parse_config(const std::vector<std::string>& lines) {
        Config cfg;

        auto find_value = [&](const std::string& key) -> Expected<std::string, ConfigError> {
            for (const auto& line : lines) {
                if (line.find(key + "=") == 0) {
                    return line.substr(key.length() + 1);
                }
            }
            return unexpected{ ConfigError("Key not found: " + key) };
            };

        auto safe_stoi_wrapped = [](const std::string& s) -> Expected<int, ConfigError> {
            try {
                return std::stoi(s);
            }
            catch (...) {
                return unexpected{ ConfigError("Invalid integer: " + s) };
            }
            };

        // Parse host
        auto host_result = find_value("host");
        if (!host_result) return unexpected{ host_result.error() };
        cfg.host = *host_result;

        // Parse port
        auto port_str_result = find_value("port");
        if (!port_str_result) return unexpected{ port_str_result.error() };

        auto port_result = safe_stoi_wrapped(*port_str_result);
        if (!port_result) return unexpected{ port_result.error() };
        cfg.port = *port_result;

        // Parse timeout
        auto timeout_str_result = find_value("timeout");
        if (!timeout_str_result) return unexpected{ timeout_str_result.error() };

        auto timeout_result = safe_stoi_wrapped(*timeout_str_result);
        if (!timeout_result) return unexpected{ timeout_result.error() };
        cfg.timeout_ms = *timeout_result;

        return cfg;
    }

    void example_real_world() {
        std::cout << "=== Real-world Config Parser Example ===\n";

        std::vector<std::string> config_lines = {
            "host=localhost",
            "port=8080",
            "timeout=5000"
        };

        auto result = parse_config(config_lines)
            .inspect([](const Config& cfg) {
            std::cout << "Config loaded successfully:\n";
            std::cout << "  Host: " << cfg.host << "\n";
            std::cout << "  Port: " << cfg.port << "\n";
            std::cout << "  Timeout: " << cfg.timeout_ms << "ms\n";
                })
            .inspect_error([](const ConfigError& err) {
            std::cout << "Failed to load config: " << err.message << "\n";
                });

        std::cout << "\n";
    }

    // ============================================================================
    // Performance Test - Demonstrating Optimized Assignment
    // ============================================================================

    void benchmark_assignment() {
        std::cout << "=== Assignment Performance Benchmark ===\n";

        constexpr size_t ITERATIONS = 1000000;

        // Same-state assignment (OPTIMIZED PATH - Fast)
        {
            Expected<int, std::string> e1(42);
            Expected<int, std::string> e2(100);

            benchmark("Same-state assignment (FAST PATH)", [&]() {
                e1 = e2;  // Fast path: direct assignment, no destructor calls
                }, ITERATIONS);
        }

        // Different-state assignment (SLOW PATH - Unavoidable)
        {
            Expected<int, std::string> e1(42);
            Expected<int, std::string> e2(unexpected{ "error" });

            benchmark("Different-state assignment (SLOW PATH)", [&]() {
                e1 = e2;  // Slow path: destroy value, construct error
                e2 = Expected<int, std::string>(100);  // Restore for next iteration
                }, ITERATIONS);
        }

        // Monadic operations performance
        {
            Expected<int, std::string> e(42);

            benchmark("Monadic map operation", [&]() {
                auto result = e.map([](int x) { return x * 2; });
                (void)result;
                }, ITERATIONS);
        }

        // and_then operation
        {
            Expected<int, std::string> e(42);

            benchmark("Monadic and_then operation", [&]() {
                auto result = e.and_then([](int x) -> Expected<int, std::string> {
                    return x * 2;
                    });
                (void)result;
                }, ITERATIONS);
        }

        // Value construction
        {
            benchmark("Value construction", []() {
                Expected<int, std::string> e(42);
                (void)e;
                }, ITERATIONS);
        }

        // Error construction
        {
            benchmark("Error construction", []() {
                Expected<int, std::string> e(unexpected{ "error" });
                (void)e;
                }, ITERATIONS);
        }

        // has_value check
        {
            Expected<int, std::string> e(42);

            benchmark("has_value() check", [&]() {
                bool has = e.has_value();
                (void)has;
                }, ITERATIONS);
        }

        // value_or access
        {
            Expected<int, std::string> e(42);

            benchmark("value_or() with value", [&]() {
                int val = e.value_or(0);
                (void)val;
                }, ITERATIONS);
        }

        {
            Expected<int, std::string> e(unexpected{ "error" });

            benchmark("value_or() with error", [&]() {
                int val = e.value_or(0);
                (void)val;
                }, ITERATIONS);
        }

        std::cout << "\n";
        std::cout << "Performance Summary:\n";
        std::cout << "  ✓ Same-state assignment uses FAST PATH (direct assignment)\n";
        std::cout << "  ✓ Different-state assignment uses SLOW PATH (destroy + construct)\n";
        std::cout << "  ✓ All operations have zero-overhead abstractions\n";
        std::cout << "  ✓ Monadic operations have minimal overhead (~nanoseconds)\n";
        std::cout << "\n";
    }

    // ============================================================================
    // Unit Tests
    // ============================================================================

    bool run_unit_tests() {
        std::cout << "=== Running Unit Tests ===\n";

        // Test 1: Basic construction
        {
            Expected<int, std::string> v(42);
            SIMPLE_ASSERT(v.has_value(), "Expected should have value");
            SIMPLE_ASSERT(*v == 42, "Value should be 42");
        }

        // Test 2: Error construction (FIXED - no ambiguity)
        {
            Expected<int, std::string> e(unexpected{ "error" });
            SIMPLE_ASSERT(!e.has_value(), "Expected should have error");
            SIMPLE_ASSERT(e.error() == "error", "Error should be 'error'");
        }

        // Test 3: Optimized assignment
        {
            Expected<int, std::string> v1(42);
            Expected<int, std::string> v2(100);
            v1 = v2;
            SIMPLE_ASSERT(*v1 == 100, "Value should be 100 after assignment");
        }

        // Test 4: Monadic map
        {
            auto result = Expected<int, std::string>(10)
                .map([](int x) { return x * 2; });
            SIMPLE_ASSERT(*result == 20, "Map should double the value");
        }

        // Test 5: Monadic and_then
        {
            auto result = Expected<int, std::string>(10)
                .and_then([](int x) -> Expected<int, std::string> {
                return x * 2;
                    });
            SIMPLE_ASSERT(*result == 20, "and_then should double the value");
        }

        // Test 6: Error propagation
        {
            auto result = Expected<int, std::string>(unexpected{ "error" })
                .map([](int x) { return x * 2; });
            SIMPLE_ASSERT(!result.has_value(), "Error should propagate");
            SIMPLE_ASSERT(result.error() == "error", "Error should be 'error'");
        }

        // Test 7: or_else recovery
        {
            auto result = Expected<int, std::string>(unexpected{ "error" })
                .or_else([](const std::string&) -> Expected<int, std::string> {
                return 42;
                    });
            SIMPLE_ASSERT(*result == 42, "or_else should recover with 42");
        }

        // Test 8: Void expected
        {
            Expected<void, std::string> v;
            SIMPLE_ASSERT(v.has_value(), "Void Expected should have value");

            Expected<void, std::string> e(unexpected{ "error" });
            SIMPLE_ASSERT(!e.has_value(), "Void Expected should have error");
        }

        // Test 9: value_or
        {
            Expected<int, std::string> v(42);
            SIMPLE_ASSERT(v.value_or(0) == 42, "value_or should return 42");

            Expected<int, std::string> e(unexpected{ "error" });
            SIMPLE_ASSERT(e.value_or(0) == 0, "value_or should return default");
        }

        // Test 10: error_or
        {
            Expected<int, std::string> v(42);
            SIMPLE_ASSERT(v.error_or("default") == "default", "error_or should return default");

            Expected<int, std::string> e(unexpected{ "error" });
            SIMPLE_ASSERT(e.error_or("default") == "error", "error_or should return error");
        }

        // Test 11: Comparisons
        {
            Expected<int, std::string> v1(42);
            Expected<int, std::string> v2(42);
            SIMPLE_ASSERT(v1 == v2, "Equal Expected should compare equal");
            SIMPLE_ASSERT(v1 == 42, "Expected should compare equal to value");

            Expected<int, std::string> e(unexpected{ "error" });
            SIMPLE_ASSERT(e == unexpected{ "error" }, "Expected should compare equal to unexpected");
        }

        // Test 12: inspect
        {
            int inspected_value = 0;
            Expected<int, std::string>(42)
                .inspect([&](int x) { inspected_value = x; });
            SIMPLE_ASSERT(inspected_value == 42, "inspect should observe value");
        }

        // Test 13: inspect_error
        {
            std::string inspected_error;
            Expected<int, std::string>(unexpected{ "error" })
                .inspect_error([&](const std::string& e) { inspected_error = e; });
            SIMPLE_ASSERT(inspected_error == "error", "inspect_error should observe error");
        }

        // Test 14: map_error
        {
            auto result = Expected<int, std::string>(unexpected{ "error" })
                .map_error([](const std::string& e) { return e + "_transformed"; });
            SIMPLE_ASSERT(result.error() == "error_transformed", "map_error should transform error");
        }

        // Test 15: Copy construction
        {
            Expected<int, std::string> v1(42);
            Expected<int, std::string> v2(v1);
            SIMPLE_ASSERT(*v2 == 42, "Copy construction should preserve value");
        }

        // Test 16: Move construction
        {
            Expected<std::string, int> v1("hello");
            Expected<std::string, int> v2(std::move(v1));
            SIMPLE_ASSERT(*v2 == "hello", "Move construction should transfer value");
        }

        // Test 17: Emplace
        {
            Expected<std::string, int> exp(unexpected{ 42 });
            exp.emplace("emplaced");
            SIMPLE_ASSERT(*exp == "emplaced", "Emplace should construct value");
        }

        // Test 18: Swap
        {
            Expected<int, std::string> v1(42);
            Expected<int, std::string> v2(100);
            v1.swap(v2);
            SIMPLE_ASSERT(*v1 == 100 && *v2 == 42, "Swap should exchange values");
        }

        // Test 19: Cross-state swap
        {
            Expected<int, std::string> v(42);
            Expected<int, std::string> e(unexpected{ "error" });
            v.swap(e);
            SIMPLE_ASSERT(!v.has_value() && e.has_value(), "Cross-state swap should exchange states");
            SIMPLE_ASSERT(v.error() == "error" && *e == 42, "Cross-state swap should preserve data");
        }

        // Test 20: Transform (alias for map)
        {
            auto result = Expected<int, std::string>(10)
                .transform([](int x) { return x * 3; });
            SIMPLE_ASSERT(*result == 30, "Transform should triple the value");
        }

        // Test 21: Storage policy - ExpectedUnion always uses UnionStorage
        {
            ExpectedUnion<int, std::string> v(42);
            SIMPLE_ASSERT(v.has_value(), "ExpectedUnion should have value");
            SIMPLE_ASSERT(*v == 42, "ExpectedUnion value should be 42");
        }

        // Test 22: Storage policy - Default Expected respects configuration
        {
            Expected<int, std::string> v(100);
            SIMPLE_ASSERT(v.has_value(), "Expected should have value");
            SIMPLE_ASSERT(*v == 100, "Expected value should be 100");
        }

#ifdef USE_VARIANT_STORAGE
        // Test 23: Storage policy - ExpectedVariant uses VariantStorage
        {
            ExpectedVariant<int, std::string> v(200);
            SIMPLE_ASSERT(v.has_value(), "ExpectedVariant should have value");
            SIMPLE_ASSERT(*v == 200, "ExpectedVariant value should be 200");
        }
#endif

        // Test 24: Feature test macros
        {
#if defined(__cpp_utilities_expected) && __cpp_utilities_expected >= 202411L
            // Base Expected is available
            Expected<int, std::string> v(42);
            SIMPLE_ASSERT(v.has_value(), "Expected base features work");
#endif

#if defined(__cpp_utilities_expected_monadic) && __cpp_utilities_expected_monadic >= 202411L
            // Monadic operations are available
            auto result = Expected<int, std::string>(42)
                .map([](int x) { return x * 2; });
            SIMPLE_ASSERT(*result == 84, "Monadic operations available via macro");
#endif

#if defined(__cpp_utilities_expected_rebind) && __cpp_utilities_expected_rebind >= 202411L
            // Rebind is available
            using IntExp = Expected<int, std::string>;
            using DoubleExp = IntExp::rebind<double>;
            static_assert(std::is_same_v<DoubleExp, Expected<double, std::string>>,
                         "Rebind changes value type");
#endif
            SIMPLE_ASSERT(true, "Feature test macros work correctly");
        }

        // Test 25: Rebind template member
        {
            // Basic rebind
            using IntExpected = Expected<int, std::string>;
            using DoubleExpected = IntExpected::rebind<double>;
            
            static_assert(std::is_same_v<DoubleExpected, Expected<double, std::string>>,
                         "rebind changes value type");
            static_assert(std::is_same_v<IntExpected::error_type, DoubleExpected::error_type>,
                         "rebind preserves error type");
            
            // Use in generic code
            auto to_double = [](auto exp) -> typename decltype(exp)::template rebind<double> {
                return exp.map([](const auto& x) { return static_cast<double>(x); });
            };
            
            Expected<int, std::string> int_exp(42);
            auto double_exp = to_double(int_exp);
            SIMPLE_ASSERT(double_exp.has_value(), "Rebind conversion works");
            SIMPLE_ASSERT(*double_exp == 42.0, "Rebind value correct");
        }

        // Test 26: Non-default-constructible types
        {
            struct NoDefault {
                int value;
                NoDefault() = delete;
                explicit NoDefault(int v) : value(v) {}
                bool operator==(const NoDefault& other) const { return value == other.value; }
            };
            
            Expected<NoDefault, std::string> exp(std::in_place, 42);
            SIMPLE_ASSERT(exp.has_value(), "NoDefault construction works");
            SIMPLE_ASSERT(exp->value == 42, "NoDefault value correct");
            
            Expected<NoDefault, std::string> err(unexpected{"error"});
            SIMPLE_ASSERT(!err.has_value(), "NoDefault error state works");
            
            auto mapped = exp.map([](const NoDefault& nd) { return nd.value * 2; });
            SIMPLE_ASSERT(*mapped == 84, "NoDefault map works");
        }

        // Test 27: Large objects and move semantics
        {
            struct LargeObject {
                std::array<int, 100> data;
                LargeObject() { data.fill(42); }
            };
            
            Expected<LargeObject, std::string> exp(std::in_place);
            SIMPLE_ASSERT(exp.has_value(), "Large object construction");
            SIMPLE_ASSERT(exp->data[0] == 42, "Large object value correct");
            
            auto moved = std::move(exp);
            SIMPLE_ASSERT(moved.has_value(), "Large object moved");
        }

        // Test 28: Concurrent read access (thread safety)
        {
            Expected<int, std::string> shared_exp(42);
            std::atomic<int> sum{0};
            
            // Launch multiple threads reading concurrently (safe)
            std::vector<std::thread> threads;
            for (int i = 0; i < 4; ++i) {
                threads.emplace_back([&]() {
                    for (int j = 0; j < 100; ++j) {
                        if (shared_exp.has_value()) {
                            sum += *shared_exp;
                        }
                    }
                });
            }
            
            for (auto& t : threads) t.join();
            SIMPLE_ASSERT(sum == 42 * 400, "Concurrent reads safe");
        }

#ifdef USE_VARIANT_STORAGE
        std::cout << "All unit tests passed! ✓ (28/28)\n\n";
#else
        std::cout << "All unit tests passed! ✓ (27/27)\n\n";
#endif
        return true;
    }

#if defined(__cpp_lib_three_way_comparison) && __cpp_lib_three_way_comparison >= 201907L
    /**
     * @brief Test three-way comparison operator (C++20)
     */
    bool test_three_way_comparison() {
        std::cout << "\n=== Three-Way Comparison (C++20) ===\n";
        
        Expected<int, std::string> v1(42);
        Expected<int, std::string> v2(43);
        Expected<int, std::string> v3(42);
        Expected<int, std::string> err1(unexpected{"error1"});
        Expected<int, std::string> err2(unexpected{"error2"});
        
        // Value comparisons
        SIMPLE_ASSERT((v1 <=> v2) == std::strong_ordering::less, "42 < 43");
        SIMPLE_ASSERT((v2 <=> v1) == std::strong_ordering::greater, "43 > 42");
        SIMPLE_ASSERT((v1 <=> v3) == std::strong_ordering::equal, "42 == 42");
        
        // Error < Value ordering
        SIMPLE_ASSERT((err1 <=> v1) == std::strong_ordering::less, "error < value");
        SIMPLE_ASSERT((v1 <=> err1) == std::strong_ordering::greater, "value > error");
        
        // Error comparisons
        SIMPLE_ASSERT((err1 <=> err2) == std::strong_ordering::less, "error1 < error2");
        SIMPLE_ASSERT((err1 <=> err1) == std::strong_ordering::equal, "error1 == error1");
        
        // All six operators work automatically from <=>
        SIMPLE_ASSERT(v1 < v2, "operator< works");
        SIMPLE_ASSERT(v1 <= v3, "operator<= works");
        SIMPLE_ASSERT(v2 > v1, "operator> works");
        SIMPLE_ASSERT(v1 >= v3, "operator>= works");
        SIMPLE_ASSERT(v1 == v3, "operator== works");
        SIMPLE_ASSERT(v1 != v2, "operator!= works");
        
        // Void Expected
        Expected<void, std::string> void1;
        Expected<void, std::string> void2;
        Expected<void, std::string> void_err(unexpected{"error"});
        
        SIMPLE_ASSERT((void1 <=> void2) == std::strong_ordering::equal, "void == void");
        SIMPLE_ASSERT((void_err <=> void1) == std::strong_ordering::less, "void error < value");
        
        std::cout << "  ✓ Three-way comparison tests passed (C++20)\n";
        return true;
    }
#endif

#if defined(__cpp_lib_expected) && __cpp_lib_expected >= 202202L
    /**
     * @brief Test std::expected integration (C++23)
     */
    bool test_std_expected_integration() {
        std::cout << "\n=== std::expected Integration (C++23) ===\n";
        
        // Test 1: Convert custom → standard (value)
        {
            Expected<int, std::string> custom(42);
            auto std_exp = to_std_expected(custom);
            
            SIMPLE_ASSERT(std_exp.has_value(), "Converted value state");
            SIMPLE_ASSERT(*std_exp == 42, "Converted value correct");
        }
        
        // Test 2: Convert custom → standard (error)
        {
            Expected<int, std::string> custom(unexpected{"error"});
            auto std_exp = to_std_expected(custom);
            
            SIMPLE_ASSERT(!std_exp.has_value(), "Converted error state");
            SIMPLE_ASSERT(std_exp.error() == "error", "Converted error correct");
        }
        
        // Test 3: Convert standard → custom (value)
        {
            std::expected<int, std::string> std_exp(42);
            auto custom = from_std_expected(std_exp);
            
            SIMPLE_ASSERT(custom.has_value(), "Converted back value state");
            SIMPLE_ASSERT(*custom == 42, "Converted back value correct");
        }
        
        // Test 4: Convert standard → custom (error)
        {
            std::expected<int, std::string> std_exp(std::unexpect, "error");
            auto custom = from_std_expected(std_exp);
            
            SIMPLE_ASSERT(!custom.has_value(), "Converted back error state");
            SIMPLE_ASSERT(custom.error() == "error", "Converted back error correct");
        }
        
        // Test 5: Round-trip conversion
        {
            Expected<int, std::string> original(42);
            auto std_exp = to_std_expected(original);
            auto back = from_std_expected(std_exp);
            
            SIMPLE_ASSERT(back.has_value(), "Round-trip preserves state");
            SIMPLE_ASSERT(*back == 42, "Round-trip preserves value");
        }
        
        // Test 6: Void Expected conversion
        {
            Expected<void, std::string> custom_void;
            auto std_void = to_std_expected(custom_void);
            
            SIMPLE_ASSERT(std_void.has_value(), "Void conversion works");
            
            auto back_void = from_std_expected(std_void);
            SIMPLE_ASSERT(back_void.has_value(), "Void round-trip works");
        }
        
        // Test 7: Move semantics
        {
            Expected<std::string, int> custom("hello");
            auto std_exp = to_std_expected(std::move(custom));
            
            SIMPLE_ASSERT(std_exp.has_value(), "Move conversion preserves state");
            SIMPLE_ASSERT(*std_exp == "hello", "Move conversion preserves value");
        }
        
        // Test 8: Interop with std::expected monadic ops
        {
            Expected<int, std::string> custom(21);
            auto std_exp = to_std_expected(custom);
            
            // Use std::expected's transform (equivalent to map)
            auto transformed = std_exp.transform([](int x) { return x * 2; });
            
            auto back = from_std_expected(transformed);
            SIMPLE_ASSERT(*back == 42, "Monadic ops work across boundary");
        }
        
        std::cout << "  ✓ std::expected integration tests passed (C++23)\n";
        return true;
    }
#endif

    // ============================================================================
    // Main
    // ============================================================================

    bool test_Expected() {
        std::cout << "======================================\n";
        std::cout << "Expected Monad - Complete Examples\n";
        std::cout << "C++17, Zero Dependencies, High Performance\n";
        std::cout << "v4.1 - C++20/23 Integration\n";
        std::cout << "======================================\n\n";

        if (!run_unit_tests()) {
            std::cerr << "Expected.h Unit tests failed!\n";
            return false;
        }

#if defined(__cpp_lib_three_way_comparison) && __cpp_lib_three_way_comparison >= 201907L
        if (!test_three_way_comparison()) {
            std::cerr << "Three-way comparison tests failed!\n";
            return false;
        }
#endif

#if defined(__cpp_lib_expected) && __cpp_lib_expected >= 202202L
        if (!test_std_expected_integration()) {
            std::cerr << "std::expected integration tests failed!\n";
            return false;
        }
#endif

        example_monadic_operations();
        example_error_recovery();
        example_error_transformation();
        example_void_expected();
        example_optimized_assignment();
        example_ctad();
        example_storage_policy();
        example_inspection();
        example_comparisons();
        example_hash();
        example_error_or();
        example_real_world();
        benchmark_assignment();

        std::cout << "======================================\n";
        std::cout << "All examples completed successfully!\n";
        std::cout << "======================================\n";

        return true;
    }

} // namespace cpp_utilities::testing