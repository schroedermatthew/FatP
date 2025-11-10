#include <iostream>
#include <string>

#include "DebugOnly.h"
#include "test_DebugOnly.h"
#include "test_Utilities.h"

namespace cpp_utilities::testing
{

bool test_debug_only_basic_construction() {
    
#ifndef NDEBUG
    DebugOnly<int> debug_val;
    DebugOnly<int> debug_val_init(42);
    SIMPLE_ASSERT(debug_val.get() == 0, "Default debug value should be 0");
    SIMPLE_ASSERT(debug_val_init.get() == 42, "Debug value should be 42");
#endif
    
    return true;
}

bool test_debug_only_assignment() {
    DebugOnly<int> val;
    val = 100;
    
    #ifndef NDEBUG
    SIMPLE_ASSERT(val.get() == 100, "Debug value should be 100");
    #endif
    
    return true;
}

bool test_debug_only_string_storage() {
    DebugOnly<std::string> debug_str("test");
    
    #ifndef NDEBUG
    SIMPLE_ASSERT(debug_str.get() == "test", "Debug string should be 'test'");
    #endif
    
    debug_str = std::string("updated");
    
    #ifndef NDEBUG
    SIMPLE_ASSERT(debug_str.get() == "updated", "Debug string should be 'updated'");
    #endif
    
    return true;
}

bool test_debug_only_complex_type() {
    struct Complex {
        int a, b;
        Complex(int x, int y) : a(x), b(y) {}
    };
    
    DebugOnly<Complex> debug_complex(Complex(10, 20));
    
    #ifndef NDEBUG
    SIMPLE_ASSERT(debug_complex.get().a == 10, "Field a should be 10");
    SIMPLE_ASSERT(debug_complex.get().b == 20, "Field b should be 20");
    #endif
    
    return true;
}

bool test_debug_only_zero_cost() {
    struct WithDebug {
        int data;
#if __cplusplus >= 202002L
        [[no_unique_address]]
#endif
        DebugOnly<std::string> debug_info;
    };
    
    #ifdef NDEBUG
    // In release:
    // - C++20 with [[no_unique_address]]: sizeof(WithDebug) == sizeof(int)
    // - C++17: sizeof(WithDebug) == sizeof(int) + padding (typically 8 bytes on 64-bit due to alignment)
    // The test should be realistic about C++ version limitations
    
#if __cplusplus >= 202002L
    // With C++20 [[no_unique_address]], we get true zero-overhead
    SIMPLE_ASSERT(sizeof(WithDebug) == sizeof(int), 
                  "C++20: Should have zero overhead with [[no_unique_address]]");
#else
    // In C++17, empty classes still take at least 1 byte, and struct padding applies
    // On most platforms: sizeof(WithDebug) == 8 (4 bytes int + 4 bytes padding/empty class)
    SIMPLE_ASSERT(sizeof(WithDebug) <= sizeof(int) + sizeof(void*), 
                  "C++17: Should have minimal overhead (empty class + padding)");
    
    // Document the actual size for transparency
    std::cout << "  [INFO] C++17 Release: sizeof(WithDebug) = " << sizeof(WithDebug) 
              << " bytes (int=" << sizeof(int) << " + empty class overhead)\n";
#endif
    
#else
    // In debug, should include full string
    SIMPLE_ASSERT(sizeof(WithDebug) > sizeof(int), "Should have debug info in debug build");
#endif
    
    return true;
}

void benchmark_debugonly() {
    std::cout << "\n" << colors::cyan() << "DebugOnly Benchmarks:" << colors::reset() << "\n\n";
    
    #ifdef NDEBUG
    std::cout << "Running in RELEASE mode (NDEBUG defined)\n";
    #else
    std::cout << "Running in DEBUG mode\n";
    #endif
    
    std::cout << "C++ Standard: ";
#if __cplusplus >= 202002L
    std::cout << "C++20 or later (supports [[no_unique_address]])\n";
#elif __cplusplus >= 201703L
    std::cout << "C++17\n";
#else
    std::cout << "C++14 or earlier\n";
#endif
    
    DebugOnly<int> debug_val;
    
    // Benchmark assignment (should be zero cost in release)
    double assign_time = measure_perf([&debug_val, i=0]() mutable {
        debug_val = i;
        ++i;
    }, 100000, 1000);
    std::cout << "Assignment: " << format_time(assign_time) << "\n";
    
    struct WithDebug {
        int data;
#if __cplusplus >= 202002L
        [[no_unique_address]]
#endif
        DebugOnly<std::string> debug_info;
    };
    
    std::cout << "Size of int: " << sizeof(int) << " bytes\n";
    std::cout << "Size of WithDebug: " << sizeof(WithDebug) << " bytes\n";
    
#ifdef NDEBUG
    #if __cplusplus >= 202002L
    std::cout << "  ✓ C++20: True zero-overhead with [[no_unique_address]]\n";
    #else
    std::cout << "  ℹ C++17: Minimal overhead (empty class = 1 byte + padding)\n";
    std::cout << "  → Upgrade to C++20 for true zero-overhead\n";
    #endif
#endif
}

bool test_DebugOnly() {

    PRINT_HEADER(DEBUG ONLY)

    TestRunner runner;

    RUN_TEST(runner, debug_only_basic_construction);
    RUN_TEST(runner, debug_only_assignment);
    RUN_TEST(runner, debug_only_string_storage);
    RUN_TEST(runner, debug_only_complex_type);
    RUN_TEST(runner, debug_only_zero_cost);

    benchmark_debugonly();

    return 0 == runner.print_summary();
}

} // namespace cpp_utilities::testing
