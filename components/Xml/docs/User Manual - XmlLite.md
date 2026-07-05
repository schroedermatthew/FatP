---
doc_id: UM-XMLLITE-001
doc_type: "User Manual"
title: "XmlLite"
fatp_components: ["XmlLite"]
topics: ["XML parsing", "from_xml deserialization", "FATP_XML_DEFINE_TYPE", "FATP_XML_ENUM_STRING_POLICY", "xml_all", "config file I/O", "XmlNode query API"]
constraints: ["leaf-only scalar elements", "no vector struct macro fields", "file-scope enum policy macro", "namespaces rejected"]
cxx_standard: "C++20"
std_equivalent: null
boost_equivalent: "Boost.PropertyTree"
build_modes: ["Debug", "Release"]
last_verified: "2026-07-05"
audience: ["C++ developers", "AI assistants"]
status: "reviewed"
---

# User Manual - XmlLite

**Scope:** Parse config XML, navigate `XmlNode`, deserialize primitives/structs/enums/vectors with `from_xml`, and generate struct deserializers with `FATP_XML_DEFINE_TYPE`.

**Not covered:**
- XML serialization / round-trip writing (parse-only today)
- Full XML features (namespaces, DTD, CDATA, XPath)
- FatPJson or EnumPlus integration (XmlLite is self-contained; see JsonLite docs for JSON)

**Prerequisites:** C++20; familiarity with `enum class` and exceptions for error handling

---

## User Manual Card

**Component:** XmlLite  
**Primary use case:** Load application configuration from XML files or strings into typed C++ structs  
**Integration pattern:** `#include "XmlLite.h"`, `parse_xml` / `parse_xml_file`, `FATP_XML_DEFINE_TYPE` for structs, `FATP_XML_ENUM_STRING_POLICY` for string enums  
**Key API:** `parse_xml()`, `parse_xml_file()`, `from_xml()`, `xml_all()`, `FATP_XML_DEFINE_TYPE`, `FATP_XML_DEFINE_TYPE_OPTIONAL`, `FATP_XML_ENUM_STRING_POLICY`  
**std equivalent:** None  
**Common mistakes:** Putting `FATP_XML_ENUM_STRING_POLICY` inside a namespace; expecting vector fields in `FATP_XML_DEFINE_TYPE`; scalar elements with child nodes; assuming integer enums validate enumerator names  
**Performance notes:** Single-pass parse into `std::string`-backed tree; suitable for config-sized documents. See `components/Xml/benchmarks/` when benchmarks exist.

---

## Table of Contents

1. [Getting Started](#getting-started)
2. [Parsing](#parsing)
3. [XmlNode Query API](#xmlnode-query-api)
4. [Scalar Deserialization](#scalar-deserialization)
5. [Struct Macros](#struct-macros)
6. [Optional Fields](#optional-fields)
7. [Vectors and Repeated Elements](#vectors-and-repeated-elements)
8. [Enum Deserialization](#enum-deserialization)
9. [Error Handling](#error-handling)
10. [Parser Strictness](#parser-strictness)
11. [Best Practices](#best-practices)
12. [Troubleshooting](#troubleshooting)
13. [Summary](#summary)

---

## Getting Started

```cpp
#include "XmlLite.h"
#include <iostream>

struct Target {
    double centerX;
    double centerY;
    double sigma;
};
FATP_XML_DEFINE_TYPE(Target, centerX, centerY, sigma)

int main()
{
    const char* xml = R"(
        <config>
            <targets>
                <target><centerX>0</centerX><centerY>0</centerY><sigma>1.5</sigma></target>
            </targets>
        </config>
    )";

    auto root = fat_p::parse_xml(xml);
    auto targets = fat_p::xml_all<Target>(root, "targets", "target");
    std::cout << targets[0].sigma << '\n';
}
```

Each struct field maps to a **child element** whose tag name equals the field name. All fields in `FATP_XML_DEFINE_TYPE` are required.

---

## Parsing

### From string

```cpp
auto root = fat_p::parse_xml(std::string_view{R"(<root><item>1</item></root>)"});
```

### From file

```cpp
auto root = fat_p::parse_xml_file("config.xml");
```

`parse_xml_file` opens the file in binary mode, reads the full contents, and throws via `FATP_XML_ENFORCE` if the file cannot be opened or read.

### Prolog and BOM

- UTF-8 BOM at the start is accepted and skipped.
- At most one `<?xml ...?>` declaration is allowed; it must be well-formed.
- `<?xml-stylesheet ...?>` and other processing instructions are rejected.

---

## XmlNode Query API

| Method | Behavior |
|--------|----------|
| `child(tag)` | First matching child, or `nullptr` |
| `require(tag)` | First matching child, or throw |
| `all(tag)` | All matching children (pointers) |
| `has(tag)` | Whether any child exists |
| `path("a.b.c")` | Dotted path via chained `require` |
| `hasPath("a.b.c")` | Non-throwing dotted path check |
| `trimmedText()` | Trimmed element text |
| `attr(name)` | Optional attribute value |

Empty path segments throw (`reject_empty_dotted_path` in tests).

```cpp
const auto& item = root.require("options").require("item");
if (root.hasPath("options.advanced.flag")) { /* ... */ }
auto id = root.attr("id");  // std::optional<std::string>
```

---

## Scalar Deserialization

Supported leaf types: `std::string`, `int`, `std::int64_t`, `std::size_t`, `double`, `bool`, `enum class`, user types with `from_xml(const XmlNode&, T&)`.

**Leaf rule:** scalar `from_xml` requires the element to have **no child elements**. Text comes from `trimmedText()`.

```cpp
double x = fat_p::from_xml<double>(node.require("centerX"));
```

`double` rejects non-finite values (`nan`, `inf`). `bool` accepts `true`/`false`/`1`/`0`.

Value-returning overload:

```cpp
auto name = fat_p::from_xml<std::string>(node);  // T must be default-constructible
```

---

## Struct Macros

### Required fields — `FATP_XML_DEFINE_TYPE`

```cpp
struct SensorConfig {
    std::string model;
    double gain;
};
FATP_XML_DEFINE_TYPE(SensorConfig, model, gain)
```

XML:

```xml
<sensor>
    <model>Alpha</model>
    <gain>1.25</gain>
</sensor>
```

```cpp
SensorConfig cfg = fat_p::from_xml<SensorConfig>(root.require("sensor"));
```

- Exactly **one** child per field; duplicates throw.
- Missing required child throws.
- Nested structs work if their `from_xml` is visible (ADL / same TU).

### Optional fields — `FATP_XML_DEFINE_TYPE_OPTIONAL`

Missing children are skipped; struct member keeps its default value. At most one child per field; duplicates still throw.

**Limitation:** vector/repeated fields are **not** supported. Use the vector API below.

---

## Optional Fields

### `std::optional<T>` as a direct element

When the node itself is present, `from_xml` fills the optional:

```cpp
std::optional<int> value;
fat_p::from_xml(node, value);
```

### Optional by child tag

```cpp
std::optional<double> threshold;
fat_p::from_xml(parent, "threshold", threshold);  // nullopt if child absent
```

---

## Vectors and Repeated Elements

Collect repeated sibling elements with the same tag:

```xml
<items>
    <item><id>1</id></item>
    <item><id>2</id></item>
</items>
```

```cpp
std::vector<Item> items;
fat_p::from_xml(parent.require("items"), "item", items);
```

Convenience wrapper:

```cpp
auto items = fat_p::xml_all<Item>(root, "items", "item");
```

Duplicate siblings are **allowed** for vector collection (unlike scalar struct fields). Each matching child is deserialized independently.

---

## Enum Deserialization

### Integer form (no policy)

```cpp
enum class Mode : int { Off = 0, On = 1 };
```

```xml
<mode>1</mode>
```

`from_xml` reads the underlying integer. Values outside the underlying type range throw. **Declared enumerator membership is not validated** — any in-range integer is accepted.

### String form — `FATP_XML_ENUM_STRING_POLICY`

Define the policy at **global file scope** (not inside any namespace):

```cpp
enum class Mode { Off, On };
FATP_XML_ENUM_STRING_POLICY(Mode, Off, On)
```

For enums in a user namespace, pass the qualified type:

```cpp
namespace app { enum class Mode { Off, On }; }
FATP_XML_ENUM_STRING_POLICY(app::Mode, Off, On)
```

XML token must match enumerator spelling exactly (`On`, not `on`):

```xml
<mode>On</mode>
```

Inside structs:

```cpp
struct Config { Mode mode; };
FATP_XML_DEFINE_TYPE(Config, mode)
```

**Alias:** If EnumPlus is not included, `FATP_ENUM_STRING_POLICY` is defined as an alias to `FATP_XML_ENUM_STRING_POLICY`.

**Policy vs integer:** Once `FATP_XML_ENUM_STRING_POLICY` is defined for an enum, numeric text like `<mode>1</mode>` throws.

---

## Error Handling

XmlLite uses `FATP_XML_ENFORCE(condition, ...)` which throws `std::runtime_error` with file, line, function, and message context.

Typical failure modes:

| Situation | Result |
|-----------|--------|
| Malformed XML | Parse throw |
| Missing required element | `missing required element:` |
| Duplicate struct field child | `duplicate element:` |
| Scalar element with children | `element has child elements during scalar conversion` |
| Invalid enum token | `invalid enum in element:` |
| Invalid numeric text | `invalid double/int in element:` |

There is no error-code or `Expected` variant in XmlLite today.

---

## Parser Strictness

The parser intentionally rejects input that is valid in full XML processors but undesirable for config:

- Prefixed element or attribute names (`<ns:tag>`)
- `xmlns` attributes
- Multiple root elements or trailing non-comment garbage
- Unclosed elements
- Duplicate attribute names on one element
- Raw `<` inside attribute values
- Non-XML whitespace (e.g. form feed) after root close

Namespaces are not supported. Use local tag names only.

---

## Best Practices

1. **Keep config XML simple:** one text value or children per element, not mixed sequences.
2. **Use string enum policies** when XML tokens must match a fixed vocabulary.
3. **Use integer enums** only when numeric values are intentional and range checks suffice.
4. **Place enum policies at file scope** after enum definitions.
5. **Use `xml_all` for lists**, not struct macros.
6. **Validate paths** with `hasPath` before optional navigation when control flow should avoid exceptions.

---

## Troubleshooting

### GCC `-Wdangling-reference` on struct macros

Ensure you are on a revision that passes child tags as `std::string_view` to `require_unique_child` (fixed in `c6b7d2b5`). Update `XmlLite.h` if macros still use `const auto&` with `#field` passed to `const std::string&`.

### `FATP_XML_ENUM_STRING_POLICY` compile errors inside namespace

Move the macro to **file scope**. The macro opens `namespace fat_p { namespace xml_detail { ... } }` relative to the call site; inside a user namespace it creates the wrong specialization target.

### MSVC C2888 on enum policy in test namespace

Same fix: file scope only. Do not place the macro inside `fat_p::testing::xmllite` or similar.

### `element has child elements during scalar conversion`

The element has nested tags but you called scalar `from_xml`. Use struct `from_xml` for nested types, or navigate to the leaf child first.

### Vector field in struct macro does not compile / misbehaves

`FATP_XML_DEFINE_TYPE` does not support `std::vector` fields. Deserialize the vector in `from_xml` manually or after parsing the parent node.

---

## Summary

XmlLite provides config-focused XML parsing and deserialization in one self-contained header:

- **Parse:** `parse_xml`, `parse_xml_file`
- **Navigate:** `XmlNode::child`, `require`, `path`, `attr`
- **Deserialize:** `from_xml`, `xml_all`
- **Structs:** `FATP_XML_DEFINE_TYPE`, `FATP_XML_DEFINE_TYPE_OPTIONAL`
- **Enums:** integer underlying type, or `FATP_XML_ENUM_STRING_POLICY` for string tokens

For design history and rejected FatPXml split, see `Design Note - XmlLite Enum Support.md`.