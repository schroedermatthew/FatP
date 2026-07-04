/**
 * @file test_XmlLite.cpp
 * @brief Unit tests for XmlLite.h
 */
/*
FATP_META:
  meta_version: 1
  component: XmlLite
  file_role: test
  path: components/Xml/tests/test_XmlLite.cpp
  layer: Testing
  namespace: fat_p
  summary: "Unit tests for XmlLite."
  api_stability: in_work
  related:
    docs_search: "XmlLite"
    headers:
      - include/fat_p/XmlLite.h
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

#include <string>

#include "FatPTest.h"
#include "XmlLite.h"

namespace fat_p::testing::xmllite
{

FATP_TEST_CASE(parse_simple_config)
{
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<nobs>
  <template>
    <classname>SimpleLadderTemplate</classname>
    <CCW>true</CCW>
  </template>
  <sensor>
    <model>GaussianSensor</model>
    <options>
      <A>0.62</A>
      <mu>50.0</mu>
    </options>
  </sensor>
</nobs>)";

    auto root = parse_xml(xml);
    FATP_ASSERT_TRUE(root.tag == "nobs", "root tag");
    FATP_ASSERT_TRUE(root.require("template").require("classname").text == "SimpleLadderTemplate",
                     "classname text");
    FATP_ASSERT_TRUE(from_xml<bool>(root.require("template").require("CCW")), "CCW bool");
    FATP_ASSERT_TRUE(from_xml<double>(root.require("sensor").require("options").require("A")) == 0.62,
                     "sensor A");

    return true;
}

FATP_TEST_CASE(reject_prefixed_element)
{
    FATP_ASSERT_THROWS(parse_xml("<cfg:port>8080</cfg:port>"),
                       std::runtime_error,
                       "prefixed element should throw");
    return true;
}

FATP_TEST_CASE(reject_xmlns_attribute)
{
    FATP_ASSERT_THROWS(parse_xml(R"(<root xmlns="http://example.com"/> )"),
                       std::runtime_error,
                       "xmlns attribute should throw");
    return true;
}

FATP_TEST_CASE(reject_prefixed_xmlns_attribute)
{
    FATP_ASSERT_THROWS(parse_xml(R"(<root xmlns:cfg="http://example.com"/> )"),
                       std::runtime_error,
                       "xmlns:prefix attribute should throw");
    return true;
}

FATP_TEST_CASE(reject_prefixed_attribute)
{
    FATP_ASSERT_THROWS(parse_xml(R"(<root xlink:href="doc.xml"/> )"),
                       std::runtime_error,
                       "prefixed attribute should throw");
    return true;
}

} // namespace fat_p::testing::xmllite

namespace fat_p::testing
{

bool test_XmlLite()
{
    FATP_PRINT_HEADER(XML LITE)

    TestRunner runner;

    FATP_RUN_TEST_NS(runner, xmllite, parse_simple_config);
    FATP_RUN_TEST_NS(runner, xmllite, reject_prefixed_element);
    FATP_RUN_TEST_NS(runner, xmllite, reject_xmlns_attribute);
    FATP_RUN_TEST_NS(runner, xmllite, reject_prefixed_xmlns_attribute);
    FATP_RUN_TEST_NS(runner, xmllite, reject_prefixed_attribute);

    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_XmlLite() ? 0 : 1;
}
#endif