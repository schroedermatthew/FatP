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

#include <optional>
#include <string>
#include <vector>

#include "FatPTest.h"
#include "XmlLite.h"

namespace fat_p::testing::xmllite
{

struct InnerXmlProbe
{
    int x{};
};
FATP_XML_DEFINE_TYPE(InnerXmlProbe, x)

struct OuterXmlProbe
{
    InnerXmlProbe inner{};
};
FATP_XML_DEFINE_TYPE(OuterXmlProbe, inner)

struct OuterOptionalXmlProbe
{
    std::optional<InnerXmlProbe> inner{};
};
FATP_XML_DEFINE_TYPE_OPTIONAL(OuterOptionalXmlProbe, inner)

struct InnerListXmlProbe
{
    int id{};
};
FATP_XML_DEFINE_TYPE(InnerListXmlProbe, id)

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

FATP_TEST_CASE(reject_trailing_garbage)
{
    FATP_ASSERT_THROWS(parse_xml("<a/>garbage"), std::runtime_error, "trailing garbage");
    FATP_ASSERT_THROWS(parse_xml("<a><b>1</b></a> junk"), std::runtime_error, "trailing junk");
    FATP_ASSERT_THROWS(parse_xml("<a></a><!DOCTYPE x>"), std::runtime_error, "post-root doctype");
    return true;
}

FATP_TEST_CASE(reject_multiple_roots)
{
    FATP_ASSERT_THROWS(parse_xml("<a/><b/>"), std::runtime_error, "multiple roots");
    return true;
}

FATP_TEST_CASE(accept_trailing_whitespace_and_comment)
{
    auto root = parse_xml("<a/> <!-- trailing comment --> \n");
    FATP_ASSERT_TRUE(root.tag == "a", "root tag with trailing comment");
    return true;
}

FATP_TEST_CASE(reject_invalid_name_start)
{
    FATP_ASSERT_THROWS(parse_xml("<1bad/>"), std::runtime_error, "digit name start");
    FATP_ASSERT_THROWS(parse_xml("<.bad/>"), std::runtime_error, "dot name start");
    FATP_ASSERT_THROWS(parse_xml("<-bad/>"), std::runtime_error, "dash name start");
    return true;
}

FATP_TEST_CASE(reject_duplicate_attributes)
{
    FATP_ASSERT_THROWS(parse_xml(R"(<root a="1" a="2"/>)"),
                       std::runtime_error,
                       "duplicate attribute");
    return true;
}

FATP_TEST_CASE(mixed_content_joins_text_chunks)
{
    auto root = parse_xml("<p>Hello <b>world</b> again</p>");
    FATP_ASSERT_TRUE(root.tag == "p", "mixed content root tag");
    FATP_ASSERT_TRUE(root.text == "Hello again", "mixed content text joined");
    FATP_ASSERT_TRUE(root.require("b").text == "world", "child text preserved");
    return true;
}

FATP_TEST_CASE(reject_unclosed_elements)
{
    FATP_ASSERT_THROWS(parse_xml("<a>text"), std::runtime_error, "unclosed text element");
    FATP_ASSERT_THROWS(parse_xml("<a><b/>"), std::runtime_error, "unclosed parent element");
    FATP_ASSERT_THROWS(parse_xml("<a><b></b>"), std::runtime_error, "missing parent close");
    return true;
}

FATP_TEST_CASE(reject_xml_stylesheet_processing_instruction)
{
    FATP_ASSERT_THROWS(parse_xml(R"(<?xml-stylesheet href="x"?><a/>)"),
                       std::runtime_error,
                       "xml-stylesheet processing instruction");
    return true;
}

FATP_TEST_CASE(deserialize_nested_user_struct)
{
    auto root = parse_xml("<outer><inner><x>7</x></inner></outer>");
    auto value = from_xml<OuterXmlProbe>(root);
    FATP_ASSERT_TRUE(value.inner.x == 7, "nested struct field");
    return true;
}

FATP_TEST_CASE(deserialize_optional_nested_user_struct)
{
    auto root = parse_xml("<outer><inner><x>9</x></inner></outer>");
    auto value = from_xml<OuterOptionalXmlProbe>(root);
    FATP_ASSERT_TRUE(value.inner.has_value(), "optional nested present");
    FATP_ASSERT_TRUE(value.inner->x == 9, "optional nested field");
    return true;
}

FATP_TEST_CASE(deserialize_vector_of_user_struct)
{
    auto root = parse_xml("<root><item><id>1</id></item><item><id>2</id></item></root>");
    std::vector<InnerListXmlProbe> items;
    from_xml(root, "item", items);
    FATP_ASSERT_TRUE(items.size() == 2, "vector size");
    FATP_ASSERT_TRUE(items[0].id == 1 && items[1].id == 2, "vector of user structs");
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
    FATP_RUN_TEST_NS(runner, xmllite, reject_trailing_garbage);
    FATP_RUN_TEST_NS(runner, xmllite, reject_multiple_roots);
    FATP_RUN_TEST_NS(runner, xmllite, accept_trailing_whitespace_and_comment);
    FATP_RUN_TEST_NS(runner, xmllite, reject_invalid_name_start);
    FATP_RUN_TEST_NS(runner, xmllite, reject_duplicate_attributes);
    FATP_RUN_TEST_NS(runner, xmllite, mixed_content_joins_text_chunks);
    FATP_RUN_TEST_NS(runner, xmllite, reject_unclosed_elements);
    FATP_RUN_TEST_NS(runner, xmllite, reject_xml_stylesheet_processing_instruction);
    FATP_RUN_TEST_NS(runner, xmllite, deserialize_nested_user_struct);
    FATP_RUN_TEST_NS(runner, xmllite, deserialize_optional_nested_user_struct);
    FATP_RUN_TEST_NS(runner, xmllite, deserialize_vector_of_user_struct);

    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_XmlLite() ? 0 : 1;
}
#endif