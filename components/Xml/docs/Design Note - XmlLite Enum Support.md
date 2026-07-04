# Design Note — XmlLite Generic `enum class` Support

**Status:** Planned (PR0a close-tag hardening complete)
**Date:** 2026-07-04  
**Scope:** XmlLite / FatPXml / EnumPlus  
**Author:** AI-assisted design (review before implementation)

**Prerequisite:** PR0 + PR0a parser hardening must land before enum work. XmlLite is a config XML reader, not a general XML library.

---

## 0. PR0 — Parser hardening (complete)

| Issue | Fix |
|-------|-----|
| Trailing garbage / multiple roots accepted | `parse()` verifies input consumed after root (whitespace/comments allowed) |
| Invalid name starts (`<1bad/>`, `<.bad/>`) | Name-start must be alpha or `_` |
| Duplicate attributes silently overwrite | Reject duplicate attribute names |
| Mixed content lossy | Documented in XmlLite overview |
| `size_t` truncation | Guard against values above `size_t` max |

## 0a. PR0a — Close-tag hardening (complete)

| Issue | Fix |
|-------|-----|
| Unclosed non-self-closing elements accepted | `parseElement()` enforces `!atEnd()` before close-tag check; requires closing tag |
| Unchecked `advance()` at EOF | `advance()` and `expect()` throw on unexpected end |
| `<?xml-stylesheet` slips through prolog | XML declaration requires `<?xml` followed by whitespace or `?>` |

Tests live in `components/Xml/tests/test_XmlLite.cpp` (namespace, trailing content, names, duplicates, mixed content, unclosed elements, processing instructions).

---

## 1. Problem statement

XmlLite today deserializes primitives and user structs via `from_xml`, but **has no `enum class` path**. Config loaders fall back to `from_xml<std::string>` and manual string compares:

```cpp
std::string tscheme = fat_p::from_xml<std::string>(tgt.require("scheme"));
if (tscheme == "cheb") dub_scheme = "cgl";
```

Fat-P already solves enum↔string in **EnumPlus** (`EnumStringPolicy`, `named_enum`, `from_string`) and **FatPJson** wires that into `from_json`. XmlLite needs the same hook, plus a way to **define policies without retyping every string literal**.

---

## 2. Goals and non-goals

### Goals

| ID | Goal |
|----|------|
| G1 | `from_xml(const XmlNode&, E&)` for `enum class E` with string element text (`<scheme>cheb</scheme>`) |
| G2 | Works automatically inside `FATP_XML_DEFINE_TYPE` / `_OPTIONAL` (no macro changes) |
| G3 | User lists enumerator tokens **once**; XML strings default to `#token` stringification |
| G4 | Error messages include element tag, raw text, and enum type context (match XmlLite `FATP_XML_ENFORCE` style) |
| G5 | Align with FatPJson enum behavior where possible (same `EnumStringPolicy`) |
| G6 | Tests + CI coverage on `xml-lite.yml` matrix |

### Non-goals (v1)

- XML **serialization** (`to_xml` / write API) — parse-only today; defer unless round-trip configs are needed
- **Alias maps** in the parser (`cheb` → `cgl` across codebases) — application/loader concern
- **Namespaces**, attributes-for-enums, or enum-as-attribute (element text only in v1)
- Case-insensitive matching by default (optional later via `from_string_icase`)
- C++26 reflection–based auto-generation
- Migrating external config loaders in the same PR series (optional follow-up)

---

## 3. Architecture decision

### 3.1 Follow the JsonLite / FatPJson split (recommended)

| Header | Role | Dependencies | Enum support |
|--------|------|--------------|--------------|
| **XmlLite.h** | Standalone parser + primitives + struct macros | `std` only | Integer-backed `enum class` (`<tag>2</tag>`) |
| **FatPXml.h** (new) | Fat-P integrated deserialization | `XmlLite.h` + `EnumPlus.h` | `named_enum` string enums |

**Rationale:** XmlLite’s Doxygen still claims zero external dependencies (like JsonLite). FatPJson already owns “ecosystem extensions.” Duplicating that pattern keeps vendored single-file copies honest.

**Alternative (all-in XmlLite.h):** Domain may legally `#include "EnumPlus.h"` (Domain → Foundation). Update the overview to qualify “zero dependencies” as std-only standalone use. Single include is simpler but weakens the lite story.

**Recommendation:** implement string enums in **`FatPXml.h`**. Document `#include "FatPXml.h"` as the Fat-P config entry point.

### 3.2 Enum policy lives in EnumPlus, not XmlLite

XmlLite/FatPXml should **not** own the string table machinery. That belongs in **EnumPlus.h** so Json, Xml, FeatureManager, and future serializers share one definition.

```
EnumPlus.h    → FATP_ENUM_STRING_POLICY, EnumStringPolicy
FatPXml.h     → from_xml<named_enum E>
XmlLite.h     → unchanged primitive/struct API
FatPJson.h    → already has from_json<named_enum E>
```

---

## 4. EnumPlus prerequisite (PR 1)

### 4.1 `FATP_ENUM_STRING_POLICY(Enum, ...)`

Generate `EnumSizeTrait<Enum>` and `EnumStringPolicy<Enum>` from a token list:

```cpp
enum class TargetScheme { cheb, uniform, linear };

FATP_ENUM_STRING_POLICY(TargetScheme, cheb, uniform, linear)
```

Expands to (conceptually):

- `EnumSizeTrait<>::size == 3`
- `constexpr std::array<std::string_view, 3> names = {"cheb", "uniform", "linear"}`
- `to_string(E)` → `names[static_cast<size_t>(e)]` with bounds check
- `from_string(sv)` → linear search on `names`; throw `std::invalid_argument` with enum type name on miss

**Rules:**

- Enumerators must be **dense 0..N-1** (matches existing `enum_values()` / `EnumSizeTrait` model)
- String form defaults to **exact enumerator spelling** (XML `cheb` ↔ `TargetScheme::cheb`)
- Macro uses `#name` stringification — **no quoted string retyping**

### 4.2 Optional: `FATP_ENUM_STRING_POLICY_FROM_LIST(Enum, LIST_MACRO)`

X-macro for a single driving list (enum body + policy from one `LIST_MACRO`):

```cpp
#define TARGET_SCHEME_LIST(X) \
    X(cheb) \
    X(uniform)

enum class TargetScheme {
#define X(n) n,
    TARGET_SCHEME_LIST(X)
#undef X
};

FATP_ENUM_STRING_POLICY_FROM_LIST(TargetScheme, TARGET_SCHEME_LIST)
```

### 4.3 Optional: `FATP_ENUM_STRING_ALIAS(Enum, enumerator, "xml_name")`

Only when XML text ≠ enumerator token. Not required for v1.

### 4.4 Tests (`test_EnumPlus.cpp`)

- Round-trip every generated enumerator
- `from_string` throws on garbage
- `named_enum` concept satisfied
- `enum_entries()` consistent with macro

---

## 5. XmlLite / FatPXml API (PR 2)

### 5.1 String enums (`named_enum`)

Add to **FatPXml.h** (mirror `FatPJson.h` `from_json<named_enum E>`):

```cpp
template <named_enum E>
inline void from_xml(const XmlNode& node, E& value)
{
    const auto sv = node.trimmedText();
    FATP_XML_ENFORCE(!sv.empty(), "empty element for enum conversion:", node.tag);
    try {
        value = fat_p::from_string<E>(sv);
    } catch (const std::exception& e) {
        FATP_XML_ENFORCE(false,
            "invalid enum in element:", node.tag,
            "value:", std::string(sv),
            "error:", e.what());
    }
}
```

Value-returning `from_xml<E>(node)` picks this up via the existing template.

### 5.2 Integer enums (in XmlLite.h)

For XML like `<mode>2</mode>`, **XmlLite.h** provides:

```cpp
template <typename E>
    requires std::is_enum_v<E> && (!named_enum<E>)
inline void from_xml(const XmlNode& node, E& value)
{
    using U = std::underlying_type_t<E>;
    U raw{};
    from_xml(node, raw);
    value = static_cast<E>(raw);
}
```

### 5.3 `FATP_XML_DEFINE_TYPE` — no changes

`FATP_XML_FROM_FIELD_REQUIRED` already calls `from_xml(*_node, value.field)`. Once `from_xml` exists for `E`, struct fields typed as enums deserialize automatically:

```cpp
struct SensorConfig {
    SensorModel model;
    PenaltyType penalty;
};
FATP_ENUM_STRING_POLICY(SensorModel, GaussianSensor, IdentitySensor)
FATP_ENUM_STRING_POLICY(PenaltyType, Identity, Gaussian)
FATP_XML_DEFINE_TYPE(SensorConfig, model, penalty)
```

### 5.4 Optional / vector — already generic

`from_xml(parent, tag, std::vector<E>&)` and `optional<E>` work via existing templates once `from_xml(node, E&)` exists.

---

## 6. Header layout and includes

### FatPXml.h skeleton

```
#pragma once
/*
FATP_META:
  component: FatPXml
  layer: Domain
  related:
    headers: XmlLite.h, EnumPlus.h
    tests: components/Xml/tests/test_FatPXml.cpp
*/
#include "XmlLite.h"
#include "EnumPlus.h"

namespace fat_p {
  // from_xml<named_enum E>
  // (optional) from_xml<std::is_enum E> integer path
}
```

### XmlLite.h doc update (only)

Add note: for `enum class` deserialization via string names, include **FatPXml.h** and define policy with `FATP_ENUM_STRING_POLICY`.

### Standalone vendored copy

Users who copy only `XmlLite.h` out of Fat-P get parser + primitives only. Enum support requires **EnumPlus.h** + **FatPXml.h** (or hand-written `from_xml` overloads in their translation unit).

---

## 7. Error-handling contract

| Case | Behavior |
|------|----------|
| Empty element text | `FATP_XML_ENFORCE` — same as numeric |
| Unknown string | `FATP_XML_ENFORCE` wrapping `from_string` exception |
| Wrong type in struct field | Surfaces as missing child or enum error on that element |

Match FatPJson: throw `std::runtime_error` via `FATP_XML_ENFORCE`, not `Expected` (XmlLite is exception-based today).

---

## 8. Test plan

**File:** `components/Xml/tests/test_FatPXml.cpp` (recommended separate from `test_XmlLite.cpp` to keep lite CI std-only).

| Test | Input XML | Assert |
|------|-----------|--------|
| `enum_round_trip` | `<color>Green</color>` | `from_xml<Color>` == `Color::Green` |
| `enum_in_struct` | nested config with enum fields | `FATP_XML_DEFINE_TYPE` fills struct |
| `enum_invalid` | `<color>Magenta</color>` | throws `std::runtime_error` |
| `enum_empty` | `<color></color>` | throws |
| `enum_vector` | repeated child elements | vector size + values |
| `enum_optional_absent` | optional enum child missing | `nullopt` |
| `enum_optional_present` | optional enum child present | value set |

Define test enums in the test TU with `FATP_ENUM_STRING_POLICY` — do not depend on `test_EnumPlus` anonymous-namespace enums.

---

## 9. CI and verification

| Step | Action |
|------|--------|
| New workflow | `fatp-xml.yml` or extend `xml-lite.yml` to build `test_FatPXml.cpp` |
| `generate_workflows.py` | Register `FatPXml` component entry |
| Local Windows | MSVC C++20 + C++23 + icx (`setvars` + `advapi32.lib`) per SandBox `WORKFLOW.md` |
| Local Linux repro | gcc/clang via CI — authoritative for MinGW-on-Linux parity |

Build line (same pattern as `xml-lite.yml`):

```text
g++ -std=c++20 -Wall -Wextra -Wpedantic -Werror -DENABLE_TEST_APPLICATION \
    -I./include/fat_p components/Xml/tests/test_FatPXml.cpp -o test_bin
```

Update `FATP_META.related.tests` on FatPXml and EnumPlus when macros land.

---

## 10. Documentation deliverables

| Artifact | Content |
|----------|---------|
| FatPXml.h Doxygen | Enum section: macro usage, example XML, `FATP_XML_DEFINE_TYPE` example |
| XmlLite.h | Pointer to FatPXml for enums |
| EnumPlus docs | `FATP_ENUM_STRING_POLICY` reference |
| This design note | Implementation plan and open decisions |

---

## 11. PR plan (DAG)

```
PR0: XmlLite parser hardening + expanded tests
  ↓
PR0a: close-tag hardening (unclosed elements, EOF-safe advance/expect, reject xml-stylesheet)
  ↓
PR1: EnumPlus FATP_ENUM_STRING_POLICY + tests
  ↓
PR2: FatPXml.h from_xml named_enum + test_FatPXml.cpp
  ↓
PR3: Docs / workflow / metadata (FatPXml CI entry, XmlLite → FatPXml pointer)
  ↓
PR4 (optional): integer enum support in FatPXml only
  ↓
PR5 (optional): migrate internal configs to typed enums
```

| PR | Files | Est. size |
|----|-------|-----------|
| **PR0** | `XmlLite.h`, `test_XmlLite.cpp` | parser fixes + ~80 lines tests |
| **PR0a** | `XmlLite.h`, `test_XmlLite.cpp` | unclosed elements, EOF-safe lexer, PI rejection |
| **PR1** | `EnumPlus.h`, `test_EnumPlus.cpp` | ~120 lines macro + ~80 lines tests |
| **PR2** | `FatPXml.h` (new), `test_FatPXml.cpp` | ~80 + ~150 |
| **PR3** | `generate_workflows.py`, workflow yml, meta blocks | ~50 |
| **PR4** | `FatPXml.h` integer enum | ~25 |
| **PR5** | Application loaders | out of scope unless requested |

Each PR: one commit, tests green locally (MSVC + icx) + CI dispatch.

---

## 12. Usage end-state (target DX)

```cpp
#include "FatPXml.h"

enum class TargetScheme { cheb, uniform };
FATP_ENUM_STRING_POLICY(TargetScheme, cheb, uniform)

struct TargetConfig {
    TargetScheme scheme;
    double M;
};
FATP_XML_DEFINE_TYPE(TargetConfig, scheme, M)

// XML:
// <target><scheme>cheb</scheme><M>15</M></target>

auto cfg = fat_p::from_xml<TargetConfig>(root.require("target"));
// cfg.scheme == TargetScheme::cheb
```

**Aliases** (e.g. DUB `cgl` vs NewNobby `cheb`) stay in loader code or a dedicated `RemapPolicy` — not in XmlLite.

---

## 13. Open decisions (resolve before implementation)

1. **FatPXml.h vs inline in XmlLite.h** — **resolved:** split (mirror JsonLite / FatPJson).
2. **Integer enum overload** — **resolved:** FatPXml only, optional PR4.
3. **Case-insensitive enum parse** — use `from_string_icase` behind a policy flag, or strict only?
4. **Test file** — **resolved:** `test_FatPXml.cpp` separate from lite `test_XmlLite.cpp`.

---

## 14. Success criteria

- [ ] `FATP_ENUM_STRING_POLICY` — no manual string literals for default spelling
- [ ] `from_xml<Enum>` works for element text
- [ ] `FATP_XML_DEFINE_TYPE` struct with enum fields parses without custom code
- [ ] 5+ tests pass on MSVC C++20/23 and icx locally; gcc/clang on Linux CI
- [ ] `FATP_META` and workflow registered for FatPXml
- [ ] XmlLite standalone story documented (no silent EnumPlus dependency in lite header)

---

## 15. Related work (already landed)

- `include/fat_p/XmlLite.h` — parser, `from_xml` primitives, struct macros, namespace rejection
- `components/Xml/tests/test_XmlLite.cpp` — lite CI tests
- `components/Xml/docs/Design Note - XmlLite Enum Support.md` — this document
- `.github/workflows/xml-lite.yml` — CI for XmlLite lite tests

**Package layout:** Xml artifacts live under `components/Xml/` only. Do not keep copies under `components/Json/`.

Enum support builds on PR0/PR0a-hardened XmlLite; it does not replace it.