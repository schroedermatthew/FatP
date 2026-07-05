---
doc_id: DN-XMLLITE-001
doc_type: "Design Note"
title: "XmlLite Generic enum class Support"
fatp_components: ["XmlLite"]
topics: ["XML enum deserialization", "self-contained header", "FATP_XML_ENUM_STRING_POLICY", "config XML", "macro-based struct mapping"]
constraints: ["zero external dependencies", "GCC dangling-reference warnings", "header self-containment", "standalone vendored copies"]
cxx_standard: "C++20"
std_equivalent: null
boost_equivalent: null
last_verified: "2026-07-05"
audience: ["C++ developers", "AI assistants", "library maintainers"]
status: "final"
---

# Design Note - XmlLite Generic enum class Support

**Status:** Implemented (supersedes planned FatPXml / EnumPlus split)  
**Decided:** 2026-07-04  
**Last reviewed:** 2026-07-05  
**Scope:** `include/fat_p/XmlLite.h`, `components/Xml/tests/test_XmlLite.cpp`

---

## Design Note Card

**Decision:** Implement integer and string `enum class` deserialization inside self-contained `XmlLite.h` using XML-local `FATP_XML_ENUM_STRING_POLICY`, not a separate `FatPXml.h` or `EnumPlus.h` dependency.  
**Context:** Config loaders needed typed enums in `FATP_XML_DEFINE_TYPE` structs without manual string compares.  
**Options considered:** FatPXml + EnumPlus split (JsonLite pattern); all-in XmlLite with local policy macro.  
**Chosen option:** Self-contained XmlLite with `XmlEnumStringPolicy` and `FATP_XML_ENUM_STRING_POLICY`.  
**Rationale:** Keeps the lite header vendorable with std-only deps; enum policy is small and XML-specific; CI header self-containment stays honest.  
**Implications:** No `FatPXml.h`; `FATP_ENUM_STRING_POLICY` in consumer TUs aliases to `FATP_XML_ENUM_STRING_POLICY` when EnumPlus is not included.

---

## Decision

XmlLite provides `from_xml` for `enum class` types (integer and string forms) and `FATP_XML_ENUM_STRING_POLICY` entirely within `XmlLite.h`, with **no dependency on EnumPlus.h**.

---

## Context

XmlLite deserializes primitives and user structs via `from_xml`, but early versions had no `enum class` path. Application code fell back to `from_xml<std::string>` and manual compares:

```cpp
std::string scheme = fat_p::from_xml<std::string>(node.require("scheme"));
if (scheme == "cheb") { /* ... */ }
```

Fat-P already has `EnumPlus` string policies for JSON (`FatPJsonLite`) and other serializers. The original design note proposed mirroring JsonLite/FatPJson with a `FatPXml.h` extension header. During implementation, maintaining a strict **std-only self-contained** `XmlLite.h` (verified by CI header self-containment and `test_XmlLite_HeaderSelfContained.cpp`) proved more important than sharing EnumPlus machinery.

---

## Constraints

1. **Self-containment:** `XmlLite.h` must compile standalone with only standard headers and `<concepts>`.
2. **Macro struct fields:** `FATP_XML_DEFINE_TYPE` must deserialize enum fields without macro changes.
3. **Config XML semantics:** Element text only; namespaces rejected; scalar elements must be leaves.
4. **Compiler matrix:** MSVC C++20/23, GCC-13/14, Clang-16/17, strict `-Werror` including GCC `-Wdangling-reference`.
5. **Vendored copies:** Users who copy only `XmlLite.h` out of Fat-P should get parser, primitives, structs, and enums in one file.

---

## Options Considered

### Option A: FatPXml.h + EnumPlus (original plan)

Split parser (`XmlLite.h`) from enum support (`FatPXml.h` including `EnumPlus.h`), mirroring JsonLite / FatPJsonLite.

**Pros:** Reuses `EnumStringPolicy` / `named_enum`; one policy definition for JSON and XML.  
**Cons:** Breaks self-contained lite story; requires second header and EnumPlus in lite CI paths; FatPXml never shipped.

### Option B: Self-contained XmlLite with XML-local policy (chosen)

Add `xml_string_enum` concept, `XmlEnumStringPolicy<E>` specialization via `FATP_XML_ENUM_STRING_POLICY`, integer fallback for enums without a policy.

**Pros:** Single include; CI header check passes; no circular/ecosystem coupling; GCC/MSVC macro placement rules documented in-header.  
**Cons:** Duplicate policy macro surface vs EnumPlus (mitigated by `FATP_ENUM_STRING_POLICY` alias when EnumPlus is absent).

---

## Implementation Summary

### Parser and deserialization hardening (landed)

| Area | Behavior |
|------|----------|
| PR0 / PR0a | Trailing garbage, multiple roots, invalid names, duplicate attributes, unclosed elements, namespace rejection |
| PR A (parser) | Raw `<` in attributes; XML-specific whitespace; UTF-8 BOM; strict XML declaration (`version="1.0"` required; optional `encoding="UTF-8"`, `standalone="yes\|no"`); ASCII name chars |
| PR B (deserialize) | Leaf-only scalars; unique required struct children; duplicate scalar fields throw |
| PR C (polish) | Non-finite double rejection; path validation; `parse_xml_file` read checks; enum docs in overview |
| GCC fix | `require_unique_child` / macro use `std::string_view` + explicit `XmlNode&` (CI `77627543586` all green) |

### Enum API (landed)

**Integer enums** (no string policy):

```xml
<mode>2</mode>
```

```cpp
enum class Mode { Off, On };
// from_xml reads underlying integer; range check only, not enumerator membership
```

**String enums** (`FATP_XML_ENUM_STRING_POLICY` at **file scope only**):

```cpp
enum class Mode { Off, On };
FATP_XML_ENUM_STRING_POLICY(Mode, Off, On)
```

```xml
<mode>On</mode>
```

**Struct integration** (unchanged macros):

```cpp
struct SensorConfig { Mode mode; double gain; };
FATP_XML_DEFINE_TYPE(SensorConfig, mode, gain)
```

**Vectors:** `from_xml(parent, childTag, vector)` and `xml_all` collect repeated siblings. `xml_all` requires a unique wrapper element. `from_xml(parent, tag, optional<T>&)` rejects duplicate siblings. `FATP_XML_DEFINE_TYPE` does **not** support vector fields.

**Macro limits:** `FATP_XML_DEFINE_TYPE`, `_OPTIONAL`, and `FATP_XML_ENUM_STRING_POLICY` each support up to **50** fields or enumerator tokens.

**Compatibility alias:** If `FATP_ENUM_STRING_POLICY` is not defined by EnumPlus, XmlLite defines it as an alias to `FATP_XML_ENUM_STRING_POLICY`.

### Tests and CI

| Artifact | Count / status |
|----------|----------------|
| `test_XmlLite.cpp` | 47 unit tests (style-guide compliant) |
| `test_XmlLite_HeaderSelfContained.cpp` | 3 runtime smoke checks |
| `.github/workflows/xml-lite.yml` | 12-job matrix; all passed on `c6b7d2b5` |

---

## Consequences

### Positive

- One header for config XML: parse, structs, enums.
- Enum fields work inside `FATP_XML_DEFINE_TYPE` / `_OPTIONAL` via existing `from_xml_adl` path.
- Strict warnings and sanitizers build clean after `string_view` child-tag lookup.

### Negative

- String enum policies are separate from EnumPlus policies unless the user unifies them manually.
- `FATP_XML_ENUM_STRING_POLICY` must be at global/file scope (GCC and MSVC placement rules).
- Integer enums do not validate declared enumerator membership.

### Obligations

- Document vector limitation, 50-field macro cap, and macro scope rules in User Manual.
- Keep header self-containment test when changing macros or includes.
- Do not reintroduce `EnumPlus.h` into `XmlLite.h` without an explicit architecture change.

---

## Rejected / Deferred

| Item | Status |
|------|--------|
| `FatPXml.h` | Rejected — functionality merged into XmlLite |
| `test_FatPXml.cpp` | Not needed — enum tests in `test_XmlLite.cpp` |
| XML serialization (`to_xml`) | Deferred |
| `FATP_XML_DEFINE_TYPE_VECTOR` | Deferred |
| Enum macro reshape (qualified specialization without namespace block) | Deferred — GCC rejected qualified form |
| Application loader migrations | Out of scope |

---

## Success Criteria

- [x] `FATP_XML_ENUM_STRING_POLICY` — tokens listed once via `#enumerator` stringification
- [x] `from_xml<Enum>` works for element text (string and integer paths)
- [x] `FATP_XML_DEFINE_TYPE` struct with enum fields parses without custom code
- [x] 47 tests pass on full CI matrix (MSVC, GCC, Clang, ASan, UBSan, TSan, strict warnings)
- [x] Header self-containment verified
- [x] XmlLite standalone story documented (`Overview`, `User Manual`, this note)

---

## Related Artifacts

| Path | Role |
|------|------|
| `include/fat_p/XmlLite.h` | Public header |
| `components/Xml/tests/test_XmlLite.cpp` | Unit tests |
| `components/Xml/tests/test_XmlLite_HeaderSelfContained.cpp` | Self-containment |
| `components/Xml/docs/Overview - XmlLite.md` | Orientation |
| `components/Xml/docs/User Manual - XmlLite.md` | Usage guide |
| `.github/workflows/xml-lite.yml` | CI |

**Package layout:** Xml artifacts live under `components/Xml/` only.