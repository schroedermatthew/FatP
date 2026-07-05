/**
 * @file test_XmlLite_HeaderSelfContained.cpp
 * @brief Header self-containment test for XmlLite.h
 *
 * @details
 * Verifies that XmlLite.h is self-contained: it compiles when included
 * first (and only) in an otherwise empty TU. Double-include validates
 * #pragma once / idempotence.
 *
 * This file exists primarily to COMPILE. Runtime checks are minimal.
 */
/*
FATP_META:
  meta_version: 1
  component: XmlLite
  file_role: header_self_contained_test
  path: components/Xml/tests/test_XmlLite_HeaderSelfContained.cpp
  layer: Testing
  namespace: fat_p::testing
  summary: "Compile-only self-containment check for XmlLite.h"
  api_stability: in_work
  related:
    headers:
      - include/fat_p/XmlLite.h
  hygiene:
    pragma_once: false
    include_guard: false
    defines_total: 0
    defines_unprefixed: 0
    undefs_total: 0
    includes_windows_h: false
*/

// CRITICAL: XmlLite.h MUST be the first include (no FatPTest.h!)
#include "XmlLite.h"
#include "XmlLite.h"  // Validate idempotence (#pragma once)

#include <iostream>

// FATP_XML_ENUM_STRING_POLICY must be at file scope (not inside any namespace).
enum class HeaderProbeMode
{
    Off,
    On
};
FATP_XML_ENUM_STRING_POLICY(HeaderProbeMode, Off, On)

namespace fat_p::testing::xmllite_header_self_contained
{

struct Probe
{
    int value{};
};
FATP_XML_DEFINE_TYPE(Probe, value)

} // namespace fat_p::testing::xmllite_header_self_contained

namespace fat_p::testing
{

bool test_XmlLite_HeaderSelfContained()
{
    std::cout << "==========================================================\n";
    std::cout << "XML LITE HEADER SELF-CONTAINMENT TEST\n";
    std::cout << "==========================================================\n\n";
    std::cout << "Suite: XmlLite Header Self-Containment\n";

    using namespace xmllite_header_self_contained;

    bool all_passed = true;

    std::cout << "[COMPILE] Running: parse_xml_available ... ";
    {
        const auto root = fat_p::parse_xml("<root><value>42</value></root>");
        const bool passed = (root.tag == "root");
        std::cout << (passed ? "PASSED" : "FAILED") << "\n";
        all_passed = all_passed && passed;
    }

    std::cout << "[COMPILE] Running: from_xml_struct_macro ... ";
    {
        const auto root = fat_p::parse_xml("<cfg><value>7</value></cfg>");
        const auto cfg = fat_p::from_xml<Probe>(root);
        const bool passed = (cfg.value == 7);
        std::cout << (passed ? "PASSED" : "FAILED") << "\n";
        all_passed = all_passed && passed;
    }

    std::cout << "[COMPILE] Running: enum_string_policy ... ";
    {
        const auto root = fat_p::parse_xml("<mode>On</mode>");
        const auto mode = fat_p::from_xml<HeaderProbeMode>(root);
        const bool passed = (mode == HeaderProbeMode::On);
        std::cout << (passed ? "PASSED" : "FAILED") << "\n";
        all_passed = all_passed && passed;
    }

    std::cout << "\n=== Test Summary ===\n";
    std::cout << "Passed: " << (all_passed ? 3 : 0) << "\n";
    std::cout << "Failed: " << (all_passed ? 0 : 1) << "\n";
    std::cout << "Total:  3\n";

    return all_passed;
}

} // namespace fat_p::testing

#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_XmlLite_HeaderSelfContained() ? 0 : 1;
}
#endif