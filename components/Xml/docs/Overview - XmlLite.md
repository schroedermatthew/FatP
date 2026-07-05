---
doc_id: OV-XMLLITE-001
doc_type: "Overview"
title: "XmlLite"
fatp_components: ["XmlLite"]
topics: ["XML parsing", "config files", "macro struct deserialization", "enum class XML", "header-only parser", "zero dependencies"]
constraints: ["config XML not full XML", "no namespaces", "mixed content lossy", "exception-based errors"]
cxx_standard: "C++20"
std_equivalent: null
boost_equivalent: "Boost.PropertyTree (different model)"
last_verified: "2026-07-05"
audience: ["C++ developers", "AI assistants"]
status: "reviewed"
---

# Overview - XmlLite

## Executive Summary

XmlLite is a **zero-dependency, header-only XML library** for C++20 configuration and parameter files. It parses XML into an in-memory tree (`XmlNode`), deserializes primitives and user-defined structs via `from_xml`, and maps repeated elements to `std::vector` through `xml_all`. Struct fields are declared once with `FATP_XML_DEFINE_TYPE`; `enum class` fields deserialize from integer or string element text using `FATP_XML_ENUM_STRING_POLICY`. The design mirrors JsonLite: single public header, exception-based `FATP_XML_ENFORCE` errors, and no Fat-P ecosystem headers required.

---

## The Problem Domain

### What Goes Wrong Without Typed Config Parsing

```cpp
// Manual XML traversal — fragile and verbose
auto* schemeNode = root.child("target");
if (!schemeNode) throw std::runtime_error("missing target");
std::string scheme = schemeNode->text;
if (scheme == "cheb") { /* map manually */ }
```

| Issue | Config-loader impact |
|-------|----------------------|
| No struct mapping | Boilerplate per config type |
| Stringly-typed enums | Typos found at runtime only |
| Permissive parsers | Invalid XML accepted silently |
| Heavy XML stacks | DTDs, XPath, namespaces you do not need |

### XmlLite's Scope

XmlLite is a **config XML reader**, not a general XML processor.

**Supported:** elements, attributes, text, nesting, repeated siblings, five predefined entities (`&amp;` `&lt;` `&gt;` `&apos;` `&quot;`), comments, XML declaration, UTF-8 BOM.

**Rejected at parse time:** namespaces (prefixed names, `xmlns`), DTDs, CDATA, processing instructions beyond strict `<?xml ...?>`, trailing non-whitespace after root, multiple roots, unclosed tags, duplicate attributes, raw `<` in attribute values.

**Intentional limitation:** mixed content (text interleaved with child elements) is not preserved in document order; adjacent text chunks are trimmed and joined with spaces in the parent `text` field.

---

## Architecture

### Single Header, Std-Only Dependencies

```
XmlLite.h
├── XmlNode          (tree + query API)
├── parse_xml        (string / file)
├── from_xml         (primitives, optional, vector, enum, ADL structs)
├── FATP_XML_DEFINE_TYPE[_OPTIONAL]
└── FATP_XML_ENUM_STRING_POLICY
```

Includes: standard library + `<concepts>` only. No EnumPlus, no JsonLite, no Fat-P Foundation headers.

### Data Flow

```mermaid
flowchart LR
    A[XML string or file] --> B[parse_xml]
    B --> C[XmlNode tree]
    C --> D[from_xml / xml_all]
    D --> E[Primitives structs enums vectors]
```

### Enum Deserialization Model

| Enum kind | XML example | Mechanism |
|-----------|-------------|-----------|
| Integer | `<mode>2</mode>` | `from_xml` reads underlying type; range check only |
| String policy | `<mode>On</mode>` | `FATP_XML_ENUM_STRING_POLICY(Mode, Off, On)` |

Enums with a string policy **require** string tokens; numeric text throws. Enums without a policy accept integer underlying values only.

### Struct Macros

`FATP_XML_DEFINE_TYPE` generates `from_xml(const XmlNode&, T&)` that reads **one child element per field** (name matches field). Duplicate required children throw. Vector fields are **not** supported by the macro — use `from_xml(parent, tag, vec)` or `xml_all`.

---

## Comparison

| Capability | XmlLite | Full XML library | Manual parsing |
|------------|---------|------------------|----------------|
| Dependencies | std only | Often external | None |
| Struct mapping | Macro | Varies | Manual |
| Enum support | Built-in | Varies | Manual |
| Namespace / DTD | Rejected | Full | N/A |
| Binary size / complexity | Low | High | Lowest |

**std equivalent:** None for this combined parse + deserialize macro model.  
**Boost equivalent:** Boost.PropertyTree offers a different API and semantics; not a drop-in replacement.

---

## Verification

| Check | Status |
|-------|--------|
| Unit tests (`test_XmlLite.cpp`) | 44 tests |
| Header self-containment | 2 compile checks |
| CI matrix (`xml-lite.yml`) | MSVC, GCC, Clang, ASan, UBSan, TSan, strict warnings |

---

## Key Takeaway Card

| Principle | One-Line Summary |
|-----------|------------------|
| **Lite means std-only** | One header; no EnumPlus required for enums |
| **Config, not canonical XML** | Strict rejection of invalid or namespace-heavy input |
| **Macros for structs, APIs for vectors** | `FATP_XML_DEFINE_TYPE` for scalars/nested; `xml_all` for lists |
| **Enums: integer or policy** | `FATP_XML_ENUM_STRING_POLICY` at file scope for string tokens |

---

*XmlLite.h — Fat-P Library. See `User Manual - XmlLite.md` for integration recipes.*