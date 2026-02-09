---
doc_id: GOV-HYGIENE-001
doc_type: governance
title: "Fat-P Systemic Hygiene Policy"
fatp_components: []
topics: ["header composability", "ODR", "namespace collision", "include order", "macro hygiene", "self-contained headers"]
constraints: ["header-only library", "multiple TU inclusion", "warning cleanliness"]
cxx_standard: "C++20"
last_verified: "2025-01-09"
audience: ["C++ developers", "library maintainers", "AI assistants"]
status: "reviewed"
---

# Fat-P Systemic Hygiene Policy

**Status:** Reviewed  
**Version:** 1.0  
**Applies to:** All public Fat-P headers and their tests/benchmarks  
**Baseline:** C++20, header-only, standard library only (no external dependencies)  
**Authority:** Subordinate to the *Fat-P Library Development Guidelines*. In case of conflict, the Development Guidelines take precedence.

This policy exists to keep Fat-P **composable, predictable, and shippable** as the codebase grows. It defines hard rules that prevent the class of problems that typically kill header-only libraries at scale:

- Namespace collisions
- Include-order landmines
- ODR/redefinition failures
- Macro drift and layout instability
- "Works in isolation, breaks in real projects" integration failures

This is not a style guide. It is a **correctness and integration** policy.

---

## 1. Goals

### 1.1 Must-Have Outcomes

A Fat-P consumer must be able to:

1. Include **any subset** of public Fat-P headers in the same translation unit.
2. Include them in **any order**.
3. Build with common warning settings (ideally `-Wall -Wextra -Wpedantic`) without:
   - redefinition errors,
   - ambiguous overload errors,
   - macro redefinition warnings,
   - ODR violations.

### 1.2 Scope

This policy covers:

- Public headers (`*.h`) shipped to users
- Any helper headers that public headers include
- Tests and benchmarks (because they are often where "temporary" hacks leak into core patterns)

### 1.3 Non-Goals

- Enforcing naming aesthetics ("snake_case vs camelCase") — see Development Guidelines Section 2
- Enforcing a single layout for documentation files — see Teaching Documents Style Guide
- Solving every API ergonomics question

---

## 2. Definitions

### 2.1 Public Header

A header is **public** if it is intended to be included directly by consumers.

Public headers must be:
- Self-contained
- Include-order independent
- Warning-clean
- Stable in naming/semantics

### 2.2 Root Namespace

"Root namespace" means `namespace fat_p { ... }` without additional module scoping.

Root namespace symbols are effectively **global** to the entire library. They must be treated like ABI surface.

### 2.3 Namespace Flattening

"Flattening" is exporting nested names into the root namespace with:

```cpp
namespace fat_p {
  using some_module::Type;        // BAD: flattening
  using some_module::function;    // BAD: flattening
}
```

Flattening is the #1 cause of cross-module collisions.

### 2.4 Composability Regression

Any change that makes:
- a header no longer self-contained, or
- two headers no longer includable together, or
- include order matter

is a **composability regression** and is treated as a P0 defect.

---

## 3. Hard Rules (MUST)

### Rule A — No Root Namespace Flattening in Headers

**Public headers MUST NOT inject `using ...;` declarations into `namespace fat_p` at namespace scope** to re-export nested module symbols.

#### A.1 Forbidden

```cpp
// JsonStreamLite.h
namespace fat_p {
namespace json_stream { struct ParseError {}; }
using json_stream::ParseError;  // BAD: pollutes fat_p root
}
```

#### A.2 Allowed

Keep the symbol inside the module namespace:

```cpp
namespace fat_p::json_stream { struct ParseError {}; }  // GOOD
```

#### A.3 Allowed Convenience Pattern

Provide an *opt-in local* macro that users expand in their `.cpp`:

```cpp
// In JsonStreamLite.h
#define USING_JSON_STREAM_LITE()                  \
  using fat_p::json_stream::ParseError;           \
  using fat_p::json_stream::ParseStatus;          \
  using fat_p::json_stream::JsonStreamParser

// In user code:
#include "JsonStreamLite.h"
int main() {
  USING_JSON_STREAM_LITE();   // GOOD: local scope import
  JsonStreamParser p;
}
```

**Important:** The macro is defined in the header, but it **does not modify `namespace fat_p`**. It only expands where the user places it.

---

### Rule B — Root Namespace Symbols Are Reserved for "Core"

Only a small set of library-wide primitives may live in `fat_p` root. Everything else must live in a module namespace.

#### B.1 Core Candidates (Examples)

Core includes:
- `fat_p::Expected`
- `fat_p::enforce`
- `fat_p::ScopeGuard`
- `fat_p::CheckedArithmetic` family

Core should be minimal and intentionally curated.

#### B.2 Adding a New Root Symbol Requires Process

If you want a new symbol in the root namespace:

1. It must be unique and not likely to collide with other modules.
2. It must be justified as a true cross-cutting primitive.
3. It must be added to the **Include-All TU** (Section 6) and proven composable.

**Default policy:** Do not add new root symbols.

---

### Rule C — Every Module Owns Its Names

Every non-core component must live in a module namespace:

- `fat_p::json_stream`
- `fat_p::cbor_stream`
- `fat_p::stable_hash_map`
- `fat_p::slot_map`
- `fat_p::feature`
- etc.

#### C.1 Example: Streaming Parsers

**Good:**

```cpp
namespace fat_p::json_stream {
  enum class ParseStatus { NeedMoreInput, Done, Error };
  struct ParseError { ... };
  class JsonStreamParser { ... };
}
```

**Bad:**

```cpp
namespace fat_p {
  enum class ParseStatus { ... };   // collides across modules
  struct ParseError { ... };        // collides across modules
}
```

---

### Rule D — Headers Must Be Include-Order Independent

A public header must not compile only when included after some other header.

#### D.1 Common Include-Order Landmine

Two headers define different overload sets with the same name in `fat_p` root (or rely on unqualified lookup).

**Bad pattern:**

```cpp
// CheckedArithmetic.h
namespace fat_p { template<class To, class From> To checked_cast(From); }

// JsonLite.h
namespace fat_p { template<class To, class From> To checked_cast(From); }
// JsonLite uses checked_cast<int>(x) internally
```

This can compile or fail depending on include order.

**Required fix pattern:**
- JSON must use a module-owned name (e.g., `json_checked_cast`) or `fat_p::json_detail::checked_cast`.
- Internal calls must be fully qualified (`::fat_p::json_detail::checked_cast<int>(...)`).

---

### Rule E — No Duplicate Entity Definitions Across Headers

A symbol may only be defined in one header file if two headers can be included together.

This includes:
- Functions (even `inline` functions)
- Templates
- Helper structs in `fat_p::detail`
- Any named entity with external linkage

#### E.1 Why "inline" Is Not a Free Pass

Even `inline` functions/templates cannot be defined twice in the **same translation unit**. If header A and header B both define `detail::foo()` and the user includes both, the compile fails.

#### E.2 Required Fix Pattern

If two modules need the same helper:
- Create a single shared header (e.g., `CSRMatrixPartitioning.h`)
- Put the helper there
- Include that shared header from both modules

---

### Rule F — Macro Hygiene (Single Source of Truth)

All configuration macros must be:
- Prefixed `FATP_`
- Defined in **one** place
- Guarded with `#ifndef` to prevent redefinition
- Never redefined in multiple headers

#### F.1 Required File

Create and use a central config header:
- `FatPConfig.h`

#### F.2 Example: `FATP_NO_UNIQUE_ADDRESS`

**Good:**

```cpp
// FatPConfig.h
#ifndef FATP_NO_UNIQUE_ADDRESS
  #if defined(__has_cpp_attribute) && __has_cpp_attribute(no_unique_address)
    #define FATP_NO_UNIQUE_ADDRESS [[no_unique_address]]
  #else
    #define FATP_NO_UNIQUE_ADDRESS
  #endif
#endif
```

**Bad:**

```cpp
// DebugOnly.h
#define FATP_NO_UNIQUE_ADDRESS ...

// FastHashMap.h
#define FATP_NO_UNIQUE_ADDRESS ...   // BAD: macro redefinition
```

---

### Rule G — Public Headers Must Be Self-Contained

A public header must compile when included alone (in an otherwise empty TU):

```cpp
#include "ThatHeader.h"
int main() {}
```

This implies:
- It includes all required standard headers
- It does not rely on transitive includes
- It does not rely on macros from other headers (unless those macros are defined in the central config header)

---

### Rule H — No `using namespace` in Headers

No public header may contain:

```cpp
using namespace fat_p;
using namespace std;
```

No exceptions.

**Cross-reference:** See also Development Guidelines Section 4.9 for the complete `using` directive policy including allowed local-scope usage.

---

### Rule I — Explicit Ownership of "detail"

All internal-only helpers must live in:
- `fat_p::detail` (global internal helpers), or
- `fat_p::<module>::detail` (module-private helpers)

**Do not** put "detail" helpers in the root without a namespace (`static` functions at global scope are forbidden in public headers).

**Cross-reference:** See Development Guidelines Section 4.5 for anonymous namespace rules in headers.

---

### Rule J — No Backwards Compatibility Shims for Hygiene Fixes

When a systemic hygiene issue is fixed:
- Do not leave alias macros
- Do not leave deprecated typedefs
- Do not ship old names "for compatibility"

Fix the problem fully and update call sites/docs.

**Cross-reference:** See Development Guidelines Section 1.3 for the no-backward-compatibility philosophy.

---

## 4. Preferred Patterns (SHOULD)

These are strong recommendations; exceptions require justification.

### 4.1 Prefer Nested Module Namespaces Over Unique Global Names

Instead of inventing globally unique names like `FatPJsonStreamParseStatus`, use:
- `fat_p::json_stream::ParseStatus`

### 4.2 Prefer Fully Qualified Internal Calls

Inside headers, prefer:

```cpp
return ::fat_p::checked_add<Policy>(a, b);
```

over relying on unqualified lookup.

### 4.3 Prefer `inline constexpr` for Constants

C++17 supports inline variables:

```cpp
inline constexpr std::size_t kDefaultLimit = 1024;
```

Avoid non-inline global variables in headers.

### 4.4 Prefer Snapshot-Before-Callback in Thread-Safe Code

If callbacks are invoked outside locks (recommended), snapshot the callback list under lock to avoid races and invalidation.

---

## 5. Required Examples (Bad vs Good)

### Example 1 — Flattening Causes Collisions (JSON stream vs CBOR stream)

**Bad:**

```cpp
// JsonStreamLite.h
namespace fat_p {
namespace json_stream { struct ParseError {}; }
using json_stream::ParseError; // BAD
}
```

```cpp
// CborStreamLite.h
namespace fat_p {
namespace cbor_stream { struct ParseError {}; }
using cbor_stream::ParseError; // BAD: redefinition
}
```

**Good:**

```cpp
namespace fat_p::json_stream { struct ParseError {}; }  // GOOD
namespace fat_p::cbor_stream { struct ParseError {}; }  // GOOD
```

---

### Example 2 — Include-Order Ambiguity (`checked_cast`)

**Bad:**

```cpp
// Two different fat_p::checked_cast templates
checked_cast<int>(x); // ambiguous depending on include order
```

**Good:**

```cpp
::fat_p::checked_cast<int, ::fat_p::ThrowOnErrorPolicy>(x); // explicit
// or
::fat_p::json_detail::checked_cast<int>(x); // JSON-owned helper
```

---

### Example 3 — Duplicate Helper Defined in Multiple Headers

**Bad:**

```cpp
// CSRMatrixParallel.h defines detail::compute_balanced_partitions
// CSRMatrix_HPC.h defines detail::compute_balanced_partitions
// Include both -> redefinition error
```

**Good:**

```cpp
// CSRMatrixPartitioning.h defines compute_balanced_partitions once
// Both headers include CSRMatrixPartitioning.h
```

---

## 6. Enforcement (CI / Build Gates)

### 6.1 Include-All Compile Test (MUST)

Maintain a compile-only TU that includes **all public headers**:

- `test_include_all_headers.cpp`

This must compile on all supported toolchains.

### 6.2 Header Self-Contained Tests (MUST)

For each public header `X.h`, there must be a compile-only test TU:

- `test_include_X.cpp`

```cpp
#include "X.h"
int main() {}
```

This prevents "accidentally relied on transitive include" bugs.

### 6.3 Randomized Include Order Test (SHOULD)

Optionally generate a few include-all variants with shuffled include order. This can be as simple as 5 pre-generated permutations committed to the repo.

### 6.4 Warning Cleanliness

Public headers should be warning-clean under:
- GCC/Clang: `-Wall -Wextra -Wpedantic`
- MSVC: `/W4`

If the project enforces `-Werror`, treat any warning as a hygiene regression.

---

## 7. Review Checklist (PR Gate)

A change is not "done" until this checklist is satisfied:

### Namespace & Symbol Safety

- [ ] No new `using ...;` exported into `namespace fat_p` at namespace scope
- [ ] No new generic root names that could collide (`ParseError`, `StreamError`, etc.)
- [ ] Module namespace used for module-owned symbols
- [ ] No `using namespace` in headers

### Include & Macro Hygiene

- [ ] Header compiles when included alone
- [ ] No macro redefinitions (central config used)
- [ ] New macros are prefixed `FATP_` and guarded with `#ifndef`

### ODR & Duplication

- [ ] No new helper defined in multiple headers
- [ ] Shared helper moved to a single shared header if used in >1 module

### CI Gates

- [ ] Include-all TU compiles
- [ ] Header self-contained TU compiles

### AI / Artifact Delivery (When Changes Are Provided as Downloads)

- [ ] Response includes a `Modified Files (N)` list with repo-relative paths
- [ ] Download links include **only** those `N` modified files (no extras)

---

## 8. Migration Guidance (When Violations Are Found)

When the include-all test breaks due to a collision:

1. Identify the name collision.
2. Decide ownership:
   - Does it belong in `fat_p` root (rare)?
   - Or does it belong in a module namespace (almost always)?
3. Remove any flattening exports.
4. Rename any generic root-level helpers to module-owned names.
5. Update all call sites, docs, tests, and benchmarks.
6. Add a regression test (include-all + any minimal repro TU).

---

## 9. Summary: The One-Sentence Law

> **If two headers cannot be included together in any order, the library is broken.**

This policy exists to keep that from happening as Fat-P grows.

---

## Version History

| Version | Date | Changes |
|---------|------|---------|
| 1.0 | 2025-01-09 | Initial release with YAML front matter and cross-references to Development Guidelines |

---

*Fat-P Systemic Hygiene Policy v1.0 — January 2025*
