/**
 * @file test_Stringify_HeaderSelfContained.cpp
 * @brief Header self-containment test for Stringify.h
 *
 * @details
 * Verifies that Stringify.h is self-contained: it compiles when included
 * first (and only) in an otherwise empty TU. Double-include validates
 * #pragma once / idempotence.
 *
 * This file exists primarily to COMPILE. Runtime checks are minimal.
 */
/*
FATP_META:
  meta_version: 1
  component: Stringify
  file_role: header_self_contained_test
  path: components/Stringify/tests/test_Stringify_HeaderSelfContained.cpp
  layer: Testing
  namespace: fat_p::testing
  summary: "Compile-only self-containment check for Stringify.h"
  api_stability: stable
  related:
    headers:
      - include/fat_p/Stringify.h
  hygiene:
    pragma_once: false
    include_guard: false
    defines_total: 0
    defines_unprefixed: 0
    undefs_total: 0
    includes_windows_h: false
*/

// CRITICAL: Stringify.h MUST be the first include (no FatPTest.h, no <iostream>!)
#include "Stringify.h"
#include "Stringify.h"  // Validate idempotence (#pragma once)

// Now we can include other things for the test
#include <iostream>
#include <vector>
#include <map>
#include <optional>

namespace fat_p::testing::stringify_header_self_contained
{

// Minimal custom type to test toString() detection
struct CustomType
{
    int mValue;
    std::string toString() const { return "Custom(" + std::to_string(mValue) + ")"; }
};

} // namespace fat_p::testing::stringify_header_self_contained

namespace fat_p::testing
{

bool test_Stringify_HeaderSelfContained()
{
    std::cout << "==========================================================\n";
    std::cout << "STRINGIFY HEADER SELF-CONTAINMENT TEST\n";
    std::cout << "==========================================================\n\n";
    std::cout << "Suite: Stringify Header Self-Containment\n";

    using namespace stringify_header_self_contained;

    bool all_passed = true;

    // Test 1: Basic int stringification
    std::cout << "[COMPILE] Running: basic_int_stringify ... ";
    {
        auto result = fat_p::toString(42);
        bool passed = (result == "42");
        std::cout << (passed ? "PASSED" : "FAILED") << "\n";
        all_passed = all_passed && passed;
    }

    // Test 2: Custom type with toString()
    std::cout << "[COMPILE] Running: custom_type_stringify ... ";
    {
        CustomType ct{99};
        auto result = fat_p::toString(ct);
        bool passed = (result == "Custom(99)");
        std::cout << (passed ? "PASSED" : "FAILED") << "\n";
        all_passed = all_passed && passed;
    }

    // Test 3: Container stringification
    std::cout << "[COMPILE] Running: container_stringify ... ";
    {
        std::vector<int> vec{1, 2, 3};
        auto result = fat_p::toString(vec);
        bool passed = (result == "[1, 2, 3]");
        std::cout << (passed ? "PASSED" : "FAILED") << "\n";
        all_passed = all_passed && passed;
    }

    // Test 4: Map stringification
    std::cout << "[COMPILE] Running: map_stringify ... ";
    {
        std::map<std::string, int> m{{"a", 1}};
        auto result = fat_p::toString(m);
        bool passed = (result.find("a: 1") != std::string::npos);
        std::cout << (passed ? "PASSED" : "FAILED") << "\n";
        all_passed = all_passed && passed;
    }

    // Test 5: Optional stringification
    std::cout << "[COMPILE] Running: optional_stringify ... ";
    {
        std::optional<int> opt = 42;
        std::optional<int> empty;
        bool passed = (fat_p::toString(opt) == "42" && fat_p::toString(empty) == "nullopt");
        std::cout << (passed ? "PASSED" : "FAILED") << "\n";
        all_passed = all_passed && passed;
    }

    // Test 6: Pair/Tuple stringification
    std::cout << "[COMPILE] Running: pair_tuple_stringify ... ";
    {
        auto p = std::make_pair(1, 2);
        auto t = std::make_tuple(1, 2, 3);
        bool passed = (fat_p::toString(p) == "(1, 2)" && 
                       fat_p::toString(t) == "(1, 2, 3)");
        std::cout << (passed ? "PASSED" : "FAILED") << "\n";
        all_passed = all_passed && passed;
    }

    // Test 7: Concept availability (compile-time check)
    std::cout << "[COMPILE] Running: concepts_available ... ";
    {
        static_assert(fat_p::concepts::stringifiable<int>);
        static_assert(fat_p::concepts::streamable<int>);
        static_assert(fat_p::concepts::printable_range<std::vector<int>>);
        static_assert(!fat_p::concepts::printable_range<std::string>);
        static_assert(fat_p::concepts::has_to_string_method<CustomType>);
        std::cout << "PASSED\n";
    }

    // Test 8: toWString availability
    std::cout << "[COMPILE] Running: wstring_available ... ";
    {
        auto wresult = fat_p::toWString(42);
        bool passed = (wresult == L"42");
        std::cout << (passed ? "PASSED" : "FAILED") << "\n";
        all_passed = all_passed && passed;
    }

    // Test 9: Convenience functions available
    std::cout << "[COMPILE] Running: convenience_functions ... ";
    {
        std::string out;
        bool try_result = fat_p::tryToString(42, out);
        auto concat = fat_p::toStringConcat("a", 1, "b");
        auto padded = fat_p::toStringPadded(42, 5);
        auto formatted = fat_p::toStringFormatted(3.14, 2);
        int val = 42;
        auto ptr_str = fat_p::toStringPointer(&val);
        
        bool passed = try_result && 
                      out == "42" && 
                      concat == "a1b" &&
                      padded.size() == 5 &&
                      formatted.find("3.14") != std::string::npos &&
                      !ptr_str.empty();
        std::cout << (passed ? "PASSED" : "FAILED") << "\n";
        all_passed = all_passed && passed;
    }

    std::cout << "\n=== Test Summary ===\n";
    std::cout << "Passed: " << (all_passed ? 9 : 0) << "\n";
    std::cout << "Failed: " << (all_passed ? 0 : 1) << "\n";
    std::cout << "Total:  9\n";

    return all_passed;
}

} // namespace fat_p::testing

#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_Stringify_HeaderSelfContained() ? 0 : 1;
}
#endif
