/**
 * @file test_XmlLite.cpp
 * @brief Comprehensive unit tests for XmlLite.h
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

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

#include "XmlLite.h"
#include "FatPTest.h"

// FATP_ENUM_STRING_POLICY must live at global/file scope (not inside any namespace).
// Inside fat_p::testing::xmllite → MSVC C2888. Inside a user namespace → app::fat_p::xml_detail.
// For namespaced enums, pass the qualified type: FATP_ENUM_STRING_POLICY(app::Mode, Off, On)

enum class GlobalSchemeXmlProbe
{
    alpha,
    beta
};
FATP_ENUM_STRING_POLICY(GlobalSchemeXmlProbe, alpha, beta)

enum class ModelXmlProbe
{
    GaussianSensor,
    IdentitySensor
};
FATP_ENUM_STRING_POLICY(ModelXmlProbe, GaussianSensor, IdentitySensor)

namespace app_xml_probe
{
enum class ColorXmlProbe
{
    Red,
    Blue
};
}
FATP_ENUM_STRING_POLICY(app_xml_probe::ColorXmlProbe, Red, Blue)

struct SensorConfigXmlProbe
{
    ModelXmlProbe model{ModelXmlProbe::GaussianSensor};
};
FATP_XML_DEFINE_TYPE(SensorConfigXmlProbe, model)

namespace fat_p::testing::xmllite
{

// ============================================================================
// Helper Types (specific to this component's tests)
// ============================================================================

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

enum class ModeXmlProbe : int
{
    Off = 0,
    On = 1
};

enum class TinyEnumXmlProbe : unsigned char
{
    A = 0,
    B = 1
};

struct ConfigWithEnumXmlProbe
{
    ModeXmlProbe mode{ModeXmlProbe::Off};
};
FATP_XML_DEFINE_TYPE(ConfigWithEnumXmlProbe, mode)

struct XmlAllItemXmlProbe
{
    int id{};
};
FATP_XML_DEFINE_TYPE(XmlAllItemXmlProbe, id)

struct OptionalDefaultXmlProbe
{
    int maxIter = 100;
};
FATP_XML_DEFINE_TYPE_OPTIONAL(OptionalDefaultXmlProbe, maxIter)

[[nodiscard]] std::string nested_xml(int depth)
{
    std::string xml;
    xml.reserve(static_cast<std::size_t>(depth) * 6u + 4u);
    for (int i = 0; i < depth; ++i)
        xml += "<a>";
    xml += '1';
    for (int i = 0; i < depth; ++i)
        xml += "</a>";
    return xml;
}

// ============================================================================
// Tests
// ============================================================================

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

    const auto root = parse_xml(xml);
    FATP_ASSERT_EQ(root.tag, std::string("nobs"), "root tag");
    FATP_ASSERT_EQ(root.require("template").require("classname").text,
                   std::string("SimpleLadderTemplate"),
                   "classname text");
    FATP_ASSERT_TRUE(from_xml<bool>(root.require("template").require("CCW")), "CCW bool");
    FATP_ASSERT_CLOSE(from_xml<double>(root.require("sensor").require("options").require("A")),
                      0.62,
                      "sensor A");

    return true;
}

FATP_TEST_CASE(parse_xml_entities)
{
    const auto root = parse_xml(R"(<msg>&lt;tag&gt; &amp; &apos;hi&apos; &quot;bye&quot;</msg>)");
    FATP_ASSERT_EQ(root.text, std::string("<tag> & 'hi' \"bye\""), "predefined entities decoded");
    return true;
}

FATP_TEST_CASE(mixed_content_joins_text_chunks)
{
    const auto root = parse_xml("<p>Hello <b>world</b> again</p>");
    FATP_ASSERT_EQ(root.tag, std::string("p"), "mixed content root tag");
    FATP_ASSERT_EQ(root.text, std::string("Hello again"), "mixed content text joined");
    FATP_ASSERT_EQ(root.require("b").text, std::string("world"), "child text preserved");
    return true;
}

FATP_TEST_CASE(accept_trailing_whitespace_and_comment)
{
    const auto root = parse_xml("<a/> <!-- trailing comment --> \n");
    FATP_ASSERT_EQ(root.tag, std::string("a"), "root tag with trailing comment");
    return true;
}

FATP_TEST_CASE(parse_utf8_bom)
{
    const std::string xml = std::string("\xEF\xBB\xBF") + "<root/>";
    const auto root = parse_xml(xml);
    FATP_ASSERT_EQ(root.tag, std::string("root"), "UTF-8 BOM skipped");
    return true;
}

FATP_TEST_CASE(reject_non_xml_whitespace_after_root)
{
    FATP_ASSERT_THROWS(parse_xml(std::string("<root/>\f")),
                       std::runtime_error,
                       "form feed is not XML whitespace");
    return true;
}

FATP_TEST_CASE(reject_raw_lt_in_attribute)
{
    FATP_ASSERT_THROWS(parse_xml(R"(<root a="<bad>"/>)"),
                       std::runtime_error,
                       "raw '<' in attribute");
    return true;
}

FATP_TEST_CASE(reject_malformed_or_repeated_xml_declaration)
{
    FATP_ASSERT_THROWS(parse_xml("<?xml?><a/>"),
                       std::runtime_error,
                       "empty XML declaration");

    FATP_ASSERT_THROWS(parse_xml(R"(<?xml version="1.0"?><?xml version="1.0"?><a/>)"),
                       std::runtime_error,
                       "repeated XML declaration");

    FATP_ASSERT_THROWS(parse_xml(R"(<!--c--><?xml version="1.0"?><a/>)"),
                       std::runtime_error,
                       "XML declaration after comment");

    return true;
}

FATP_TEST_CASE(reject_excessive_nesting_depth)
{
    const auto root = parse_xml(nested_xml(200));
    FATP_ASSERT_EQ(root.tag, std::string("a"), "200-level nesting root tag");

    FATP_ASSERT_THROWS(parse_xml(nested_xml(201)),
                       std::runtime_error,
                       "XML nesting depth exceeded");
    return true;
}

FATP_TEST_CASE(reject_mismatched_closing_tag)
{
    FATP_ASSERT_THROWS(parse_xml("<a>text</b>"),
                       std::runtime_error,
                       "mismatched closing tag");
    return true;
}

FATP_TEST_CASE(reject_unknown_entity)
{
    FATP_ASSERT_THROWS(parse_xml("<a>&bogus;</a>"),
                       std::runtime_error,
                       "unknown entity");
    return true;
}

FATP_TEST_CASE(interior_comment_joins_text_chunks)
{
    const auto root = parse_xml("<a>x <!--c--> y</a>");
    FATP_ASSERT_EQ(root.text, std::string("x  y"), "comment inside text leaves gap spacing");
    return true;
}

FATP_TEST_CASE(deserialize_attribute_entity)
{
    const auto root = parse_xml(R"(<root a="&quot;x&quot;"/>)");
    const auto value = root.attr("a");
    FATP_ASSERT_TRUE(value.has_value(), "attribute should exist");
    FATP_ASSERT_EQ(*value, std::string("\"x\""), "attribute entity decoded");
    return true;
}

FATP_TEST_CASE(reject_invalid_xml_declaration)
{
    FATP_ASSERT_THROWS(parse_xml("<?xml nonsense?><a/>"),
                       std::runtime_error,
                       "XML declaration missing version");

    FATP_ASSERT_THROWS(parse_xml(R"(<?xml encoding="UTF-8"?><a/>)"),
                       std::runtime_error,
                       "XML declaration missing version");

    FATP_ASSERT_THROWS(parse_xml("<?xml version=1.0?><a/>"),
                       std::runtime_error,
                       "XML declaration attribute value must be quoted");

    FATP_ASSERT_THROWS(parse_xml(R"(<?xml version="2.0"?><a/>)"),
                       std::runtime_error,
                       "unsupported XML version");

    FATP_ASSERT_THROWS(parse_xml(R"(<?xml version="1.0" bogus="x"?><a/>)"),
                       std::runtime_error,
                       "unknown XML declaration attribute");

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

FATP_TEST_CASE(query_child_has_all)
{
    const auto root = parse_xml("<root><a>1</a><b>2</b><a>3</a></root>");

    FATP_ASSERT_NULLPTR(root.child("missing"), "child returns nullptr when absent");
    FATP_ASSERT_NOT_NULLPTR(root.child("a"), "child returns pointer when present");
    FATP_ASSERT_EQ(root.child("a")->text, std::string("1"), "first child text");

    FATP_ASSERT_FALSE(root.has("missing"), "has returns false when absent");
    FATP_ASSERT_TRUE(root.has("b"), "has returns true when present");

    const auto matches = root.all("a");
    FATP_ASSERT_EQ(matches.size(), size_t(2), "all returns every matching child");
    FATP_ASSERT_EQ(matches[0]->text, std::string("1"), "first all() match");
    FATP_ASSERT_EQ(matches[1]->text, std::string("3"), "second all() match");

    return true;
}

FATP_TEST_CASE(query_path_has_path)
{
    const auto root = parse_xml("<root><options><A>0.5</A></options></root>");

    FATP_ASSERT_TRUE(root.hasPath("options.A"), "hasPath returns true for existing dotted path");
    FATP_ASSERT_FALSE(root.hasPath("options.B"), "hasPath returns false for missing segment");

    FATP_ASSERT_CLOSE(from_xml<double>(root.path("options.A")), 0.5, "path resolves nested element");

    FATP_ASSERT_THROWS(root.path("options.B"),
                       std::runtime_error,
                       "path throws when segment missing");

    return true;
}

FATP_TEST_CASE(reject_empty_dotted_path)
{
    const auto root = parse_xml("<root/>");

    FATP_ASSERT_THROWS(root.path(""), std::runtime_error, "empty path");
    FATP_ASSERT_FALSE(root.hasPath(""), "empty path is not present");
    FATP_ASSERT_THROWS(root.path("a..b"), std::runtime_error, "empty segment");

    return true;
}

FATP_TEST_CASE(query_attr)
{
    const auto root = parse_xml(R"(<node id="42" label="probe"/> )");

    const auto id = root.attr("id");
    FATP_ASSERT_TRUE(id.has_value(), "attr returns value when present");
    FATP_ASSERT_EQ(*id, std::string("42"), "attr value");

    const auto missing = root.attr("missing");
    FATP_ASSERT_FALSE(missing.has_value(), "attr returns nullopt when absent");

    return true;
}

FATP_TEST_CASE(reject_scalar_with_child_element)
{
    FATP_ASSERT_THROWS(from_xml<int>(parse_xml("<x>1<y>2</y></x>")),
                       std::runtime_error,
                       "int scalar with child");

    FATP_ASSERT_THROWS(from_xml<bool>(parse_xml("<x>true<y>false</y></x>")),
                       std::runtime_error,
                       "bool scalar with child");

    return true;
}

FATP_TEST_CASE(reject_nonfinite_double)
{
    FATP_ASSERT_THROWS(from_xml<double>(parse_xml("<x>nan</x>")),
                       std::runtime_error,
                       "nan rejected");

    FATP_ASSERT_THROWS(from_xml<double>(parse_xml("<x>inf</x>")),
                       std::runtime_error,
                       "inf rejected");

    return true;
}

FATP_TEST_CASE(deserialize_primitive_types)
{
    const auto root = parse_xml(R"(
<values>
  <text>hello</text>
  <flag>false</flag>
  <count>17</count>
  <big>-9223372036854775807</big>
  <size>99</size>
</values>)");

    FATP_ASSERT_EQ(from_xml<std::string>(root.require("text")),
                   std::string("hello"),
                   "string primitive");
    FATP_ASSERT_FALSE(from_xml<bool>(root.require("flag")), "bool false primitive");
    FATP_ASSERT_EQ(from_xml<int>(root.require("count")), 17, "int primitive");
    FATP_ASSERT_EQ(from_xml<std::int64_t>(root.require("big")),
                   std::int64_t{-9223372036854775807},
                   "int64 primitive");
    FATP_ASSERT_EQ(from_xml<std::size_t>(root.require("size")), size_t(99), "size_t primitive");

    return true;
}

FATP_TEST_CASE(deserialize_nested_user_struct)
{
    const auto root = parse_xml("<outer><inner><x>7</x></inner></outer>");
    const auto value = from_xml<OuterXmlProbe>(root);
    FATP_ASSERT_EQ(value.inner.x, 7, "nested struct field");
    return true;
}

FATP_TEST_CASE(deserialize_optional_nested_user_struct)
{
    const auto root = parse_xml("<outer><inner><x>9</x></inner></outer>");
    const auto value = from_xml<OuterOptionalXmlProbe>(root);
    FATP_ASSERT_TRUE(value.inner.has_value(), "optional nested present");
    FATP_ASSERT_EQ(value.inner->x, 9, "optional nested field");
    return true;
}

FATP_TEST_CASE(deserialize_optional_nested_absent)
{
    const auto root = parse_xml("<outer/>");
    const auto value = from_xml<OuterOptionalXmlProbe>(root);
    FATP_ASSERT_FALSE(value.inner.has_value(), "optional nested absent keeps nullopt");
    return true;
}

FATP_TEST_CASE(deserialize_vector_of_user_struct)
{
    const auto root = parse_xml("<root><item><id>1</id></item><item><id>2</id></item></root>");
    std::vector<InnerListXmlProbe> items;
    from_xml(root, "item", items);
    FATP_ASSERT_EQ(items.size(), size_t(2), "vector size");
    FATP_ASSERT_EQ(items[0].id, 1, "first vector item");
    FATP_ASSERT_EQ(items[1].id, 2, "second vector item");
    return true;
}

FATP_TEST_CASE(deserialize_xml_all_wrapper)
{
    const auto root = parse_xml("<root><items><item><id>10</id></item><item><id>20</id></item></items></root>");
    const auto items = xml_all<XmlAllItemXmlProbe>(root, "items", "item");
    FATP_ASSERT_EQ(items.size(), size_t(2), "xml_all size");
    FATP_ASSERT_EQ(items[0].id, 10, "xml_all first item");
    FATP_ASSERT_EQ(items[1].id, 20, "xml_all second item");
    return true;
}

FATP_TEST_CASE(reject_xml_all_missing_wrapper)
{
    const auto root = parse_xml("<root><item><id>1</id></item></root>");
    FATP_ASSERT_THROWS(xml_all<XmlAllItemXmlProbe>(root, "items", "item"),
                       std::runtime_error,
                       "missing required element");
    return true;
}

FATP_TEST_CASE(reject_duplicate_xml_all_wrapper)
{
    const auto root = parse_xml(
        "<root><items><item><id>1</id></item></items>"
        "<items><item><id>2</id></item></items></root>");
    FATP_ASSERT_THROWS(xml_all<XmlAllItemXmlProbe>(root, "items", "item"),
                       std::runtime_error,
                       "duplicate xml_all wrapper");
    return true;
}

FATP_TEST_CASE(reject_duplicate_optional_child_by_tag)
{
    const auto root = parse_xml("<root><x>1</x><x>2</x></root>");
    std::optional<int> x;
    FATP_ASSERT_THROWS(from_xml(root, "x", x),
                       std::runtime_error,
                       "duplicate optional child");
    return true;
}

FATP_TEST_CASE(reject_missing_required_struct_field)
{
    FATP_ASSERT_THROWS(from_xml<OuterXmlProbe>(parse_xml("<outer/>")),
                       std::runtime_error,
                       "missing required struct field");
    return true;
}

FATP_TEST_CASE(reject_duplicate_required_struct_field)
{
    FATP_ASSERT_THROWS(
        from_xml<ConfigWithEnumXmlProbe>(
            parse_xml("<cfg><mode>0</mode><mode>1</mode></cfg>")),
        std::runtime_error,
        "duplicate required struct field");

    return true;
}

FATP_TEST_CASE(reject_parse_xml_file_missing)
{
    FATP_ASSERT_THROWS(parse_xml_file("xmllite_nonexistent_probe_9f3c2a1b.xml"),
                       std::runtime_error,
                       "cannot open file");
    return true;
}

FATP_TEST_CASE(optional_macro_keeps_member_default)
{
    const auto value = from_xml<OptionalDefaultXmlProbe>(parse_xml("<cfg/>"));
    FATP_ASSERT_EQ(value.maxIter, 100, "optional macro keeps default when child absent");
    return true;
}

FATP_TEST_CASE(parse_xml_file_roundtrip)
{
    const auto filename =
        (std::filesystem::temp_directory_path() /
         ("xmllite_parse_file_probe_" + std::to_string(std::rand()) + ".xml"))
            .string();

    {
        std::ofstream out{filename, std::ios::binary};
        FATP_ASSERT_TRUE(out.is_open(), "temp file should open for write");
        out << "<cfg><value>123</value></cfg>";
    }

    const auto root = parse_xml_file(filename);
    FATP_ASSERT_EQ(root.tag, std::string("cfg"), "parse_xml_file root tag");
    FATP_ASSERT_EQ(from_xml<int>(root.require("value")), 123, "parse_xml_file content");

    std::filesystem::remove(filename);
    return true;
}

FATP_TEST_CASE(deserialize_integer_enum)
{
    const auto root = parse_xml("<mode>1</mode>");
    const auto mode = from_xml<ModeXmlProbe>(root);
    FATP_ASSERT_EQ(static_cast<int>(mode), static_cast<int>(ModeXmlProbe::On), "integer enum value");
    return true;
}

FATP_TEST_CASE(deserialize_enum_struct_field)
{
    const auto root = parse_xml("<cfg><mode>0</mode></cfg>");
    const auto cfg = from_xml<ConfigWithEnumXmlProbe>(root);
    FATP_ASSERT_EQ(static_cast<int>(cfg.mode), static_cast<int>(ModeXmlProbe::Off), "enum struct field");
    return true;
}

FATP_TEST_CASE(deserialize_optional_enum)
{
    const auto root = parse_xml("<cfg><mode>1</mode></cfg>");
    std::optional<ModeXmlProbe> mode;
    from_xml(root.require("mode"), mode);
    FATP_ASSERT_TRUE(mode.has_value(), "optional enum present");
    FATP_ASSERT_EQ(static_cast<int>(*mode), static_cast<int>(ModeXmlProbe::On), "optional enum value");
    return true;
}

FATP_TEST_CASE(deserialize_vector_of_enums)
{
    const auto root = parse_xml("<root><mode>0</mode><mode>1</mode></root>");
    std::vector<ModeXmlProbe> modes;
    from_xml(root, "mode", modes);
    FATP_ASSERT_EQ(modes.size(), size_t(2), "enum vector size");
    FATP_ASSERT_EQ(static_cast<int>(modes[0]), static_cast<int>(ModeXmlProbe::Off), "enum vector first value");
    FATP_ASSERT_EQ(static_cast<int>(modes[1]), static_cast<int>(ModeXmlProbe::On), "enum vector second value");
    return true;
}

FATP_TEST_CASE(reject_enum_out_of_range)
{
    FATP_ASSERT_THROWS(from_xml<TinyEnumXmlProbe>(parse_xml("<x>300</x>")),
                       std::runtime_error,
                       "enum underlying overflow");
    FATP_ASSERT_THROWS(from_xml<ModeXmlProbe>(parse_xml("<mode>on</mode>")),
                       std::runtime_error,
                       "invalid enum text");
    return true;
}

FATP_TEST_CASE(deserialize_string_enum)
{
    const auto root = parse_xml("<model>GaussianSensor</model>");
    const auto model = from_xml<ModelXmlProbe>(root);
    FATP_ASSERT_EQ(static_cast<int>(model), static_cast<int>(ModelXmlProbe::GaussianSensor), "string enum value");
    return true;
}

FATP_TEST_CASE(deserialize_string_enum_struct_field)
{
    const auto root = parse_xml("<sensor><model>IdentitySensor</model></sensor>");
    const auto cfg = from_xml<SensorConfigXmlProbe>(root);
    FATP_ASSERT_EQ(static_cast<int>(cfg.model),
                   static_cast<int>(ModelXmlProbe::IdentitySensor),
                   "string enum struct field");
    return true;
}

FATP_TEST_CASE(deserialize_string_enum_global_namespace)
{
    const auto root = parse_xml("<scheme>beta</scheme>");
    const auto scheme = from_xml<GlobalSchemeXmlProbe>(root);
    FATP_ASSERT_EQ(static_cast<int>(scheme),
                   static_cast<int>(GlobalSchemeXmlProbe::beta),
                   "global namespace string enum");
    return true;
}

FATP_TEST_CASE(deserialize_string_enum_namespaced_type)
{
    const auto root = parse_xml("<color>Blue</color>");
    const auto color = from_xml<app_xml_probe::ColorXmlProbe>(root);
    FATP_ASSERT_EQ(static_cast<int>(color),
                   static_cast<int>(app_xml_probe::ColorXmlProbe::Blue),
                   "namespaced enum via qualified FATP_ENUM_STRING_POLICY type");
    return true;
}

FATP_TEST_CASE(reject_invalid_string_enum)
{
    FATP_ASSERT_THROWS(from_xml<ModelXmlProbe>(parse_xml("<model>NotASensor</model>")),
                       std::runtime_error,
                       "invalid string enum");
    return true;
}

FATP_TEST_CASE(reject_numeric_value_for_string_policy_enum)
{
    FATP_ASSERT_THROWS(from_xml<ModelXmlProbe>(parse_xml("<model>0</model>")),
                       std::runtime_error,
                       "numeric token rejected for string enum");
    return true;
}

} // namespace fat_p::testing::xmllite

// ============================================================================
// Public Interface
// ============================================================================

namespace fat_p::testing
{

bool test_XmlLite()
{
    FATP_PRINT_HEADER(XML LITE)

    TestRunner runner;
    auto& out = *get_test_config().output;

    out << colors::blue() << "--- Basic Parsing ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, xmllite, parse_simple_config);
    FATP_RUN_TEST_NS(runner, xmllite, parse_xml_entities);
    FATP_RUN_TEST_NS(runner, xmllite, mixed_content_joins_text_chunks);
    FATP_RUN_TEST_NS(runner, xmllite, accept_trailing_whitespace_and_comment);
    FATP_RUN_TEST_NS(runner, xmllite, parse_utf8_bom);
    FATP_RUN_TEST_NS(runner, xmllite, reject_non_xml_whitespace_after_root);
    FATP_RUN_TEST_NS(runner, xmllite, reject_raw_lt_in_attribute);
    FATP_RUN_TEST_NS(runner, xmllite, reject_malformed_or_repeated_xml_declaration);
    FATP_RUN_TEST_NS(runner, xmllite, reject_invalid_xml_declaration);
    FATP_RUN_TEST_NS(runner, xmllite, reject_excessive_nesting_depth);
    FATP_RUN_TEST_NS(runner, xmllite, interior_comment_joins_text_chunks);
    FATP_RUN_TEST_NS(runner, xmllite, deserialize_attribute_entity);

    out << "\n" << colors::blue() << "--- Parser Rejection ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, xmllite, reject_prefixed_element);
    FATP_RUN_TEST_NS(runner, xmllite, reject_xmlns_attribute);
    FATP_RUN_TEST_NS(runner, xmllite, reject_prefixed_xmlns_attribute);
    FATP_RUN_TEST_NS(runner, xmllite, reject_prefixed_attribute);
    FATP_RUN_TEST_NS(runner, xmllite, reject_trailing_garbage);
    FATP_RUN_TEST_NS(runner, xmllite, reject_multiple_roots);
    FATP_RUN_TEST_NS(runner, xmllite, reject_invalid_name_start);
    FATP_RUN_TEST_NS(runner, xmllite, reject_duplicate_attributes);
    FATP_RUN_TEST_NS(runner, xmllite, reject_unclosed_elements);
    FATP_RUN_TEST_NS(runner, xmllite, reject_mismatched_closing_tag);
    FATP_RUN_TEST_NS(runner, xmllite, reject_unknown_entity);
    FATP_RUN_TEST_NS(runner, xmllite, reject_xml_stylesheet_processing_instruction);

    out << "\n" << colors::blue() << "--- XmlNode Query API ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, xmllite, query_child_has_all);
    FATP_RUN_TEST_NS(runner, xmllite, query_path_has_path);
    FATP_RUN_TEST_NS(runner, xmllite, query_attr);
    FATP_RUN_TEST_NS(runner, xmllite, reject_empty_dotted_path);

    out << "\n" << colors::blue() << "--- Deserialization ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, xmllite, reject_scalar_with_child_element);
    FATP_RUN_TEST_NS(runner, xmllite, reject_nonfinite_double);
    FATP_RUN_TEST_NS(runner, xmllite, deserialize_primitive_types);
    FATP_RUN_TEST_NS(runner, xmllite, deserialize_nested_user_struct);
    FATP_RUN_TEST_NS(runner, xmllite, deserialize_optional_nested_user_struct);
    FATP_RUN_TEST_NS(runner, xmllite, deserialize_optional_nested_absent);
    FATP_RUN_TEST_NS(runner, xmllite, deserialize_vector_of_user_struct);
    FATP_RUN_TEST_NS(runner, xmllite, deserialize_xml_all_wrapper);
    FATP_RUN_TEST_NS(runner, xmllite, reject_xml_all_missing_wrapper);
    FATP_RUN_TEST_NS(runner, xmllite, reject_duplicate_xml_all_wrapper);
    FATP_RUN_TEST_NS(runner, xmllite, optional_macro_keeps_member_default);
    FATP_RUN_TEST_NS(runner, xmllite, reject_duplicate_optional_child_by_tag);
    FATP_RUN_TEST_NS(runner, xmllite, reject_missing_required_struct_field);
    FATP_RUN_TEST_NS(runner, xmllite, reject_duplicate_required_struct_field);

    out << "\n" << colors::blue() << "--- File I/O ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, xmllite, reject_parse_xml_file_missing);
    FATP_RUN_TEST_NS(runner, xmllite, parse_xml_file_roundtrip);

    out << "\n" << colors::blue() << "--- Integer Enum Deserialization ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, xmllite, deserialize_integer_enum);
    FATP_RUN_TEST_NS(runner, xmllite, deserialize_enum_struct_field);
    FATP_RUN_TEST_NS(runner, xmllite, deserialize_optional_enum);
    FATP_RUN_TEST_NS(runner, xmllite, deserialize_vector_of_enums);
    FATP_RUN_TEST_NS(runner, xmllite, reject_enum_out_of_range);

    out << "\n" << colors::blue() << "--- String Enum Deserialization ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, xmllite, deserialize_string_enum);
    FATP_RUN_TEST_NS(runner, xmllite, deserialize_string_enum_struct_field);
    FATP_RUN_TEST_NS(runner, xmllite, deserialize_string_enum_global_namespace);
    FATP_RUN_TEST_NS(runner, xmllite, deserialize_string_enum_namespaced_type);
    FATP_RUN_TEST_NS(runner, xmllite, reject_invalid_string_enum);
    FATP_RUN_TEST_NS(runner, xmllite, reject_numeric_value_for_string_policy_enum);

    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_XmlLite() ? 0 : 1;
}
#endif