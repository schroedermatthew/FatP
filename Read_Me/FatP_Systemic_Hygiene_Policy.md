# Fat‑P Systemic Hygiene Policy
**Status:** Work in progress (living document)  
**Applies to:** All public Fat‑P headers and their tests/benchmarks  
**Baseline:** C++17, header‑only, standard library only (no external dependencies)
**Authority:** Subordinate to the *Fat‑P Library Development Guidelines*. In case of conflict, the Development Guidelines take precedence.

This policy exists to keep Fat‑P **composable, predictable, and shippable** as the codebase grows. It defines hard rules that prevent the class of problems that typically kill header‑only libraries at scale:

- namespace collisions
- include‑order landmines
- ODR/redefinition failures
- macro drift and layout instability
- “works in isolation, breaks in real projects” integration failures

This is not a style guide. It is a **correctness and integration** policy.

---

## 1. Goals

### 1.1 Must‑have outcomes
A Fat‑P consumer must be able to:

1. Include **any subset** of public Fat‑P headers in the same translation unit.
2. Include them in **any order**.
3. Build with common warning settings (ideally `-Wall -Wextra -Wpedantic`) without:
   - redefinition errors,
   - ambiguous overload errors,
   - macro redefinition warnings,
   - ODR violations.

### 1.2 Scope
This policy covers:

- public headers (`*.h`) shipped to users,
- any helper headers that public headers include,
- tests and benchmarks (because they are often where “temporary” hacks leak into core patterns).

### 1.3 Non‑goals
- Enforcing naming aesthetics (“snake_case vs camelCase”).
- Enforcing a single layout for documentation files.
- Solving every API ergonomics question.

---

## 2. Definitions

### 2.1 Public header
A header is **public** if it is intended to be included directly by consumers.

Public headers must be:
- self‑contained,
- include‑order independent,
- warning‑clean,
- and stable in naming/semantics.

### 2.2 Root namespace
“Root namespace” means `namespace fat_p { … }` without additional module scoping.

Root namespace symbols are effectively **global** to the entire library. They must be treated like ABI surface.

### 2.3 Namespace flattening
“Flattening” is exporting nested names into the root namespace with:
```cpp
namespace fat_p {
  using some_module::Type;        // ❌ flattening
  using some_module::function;    // ❌ flattening
}
```

Flattening is the #1 cause of cross‑module collisions.

### 2.4 Composability regression
Any change that makes:
- a header no longer self‑contained, or
- two headers no longer includable together, or
- include order matter

is a **composability regression** and is treated as a P0 defect.

---

## 3. Hard Rules (MUST)

### Rule A — No root namespace flattening in headers
**Public headers MUST NOT inject `using …;` declarations into `namespace fat_p` at namespace scope** to re-export nested module symbols.

#### A.1 Forbidden
```cpp
// JsonStreamLite.h
namespace fat_p {
namespace json_stream { struct ParseError {}; }
using json_stream::ParseError;  // ❌ forbidden: pollutes fat_p root
}
```

#### A.2 Allowed
Keep the symbol inside the module namespace:
```cpp
namespace fat_p::json_stream { struct ParseError {}; }  // ✅ allowed
```

#### A.3 Allowed convenience pattern
Provide an *opt‑in local* macro that users expand in their `.cpp`:
```cpp
// In JsonStreamLite.h
#define USING_JSON_STREAM_LITE()                  \
  using fat_p::json_stream::ParseError;           \
  using fat_p::json_stream::ParseStatus;          \
  using fat_p::json_stream::JsonStreamParser

// In user code:
#include "JsonStreamLite.h"
int main() {
  USING_JSON_STREAM_LITE();   // ✅ local scope import
  JsonStreamParser p;
}
```

**Important:** the macro is defined in the header, but it **does not modify `namespace fat_p`**. It only expands where the user places it.

---

### Rule B — Root namespace symbols are reserved for “Core”
Only a small set of library-wide primitives may live in `fat_p` root. Everything else must live in a module namespace.

#### B.1 Core candidates (examples)
Core is things like:
- `fat_p::Expected`
- `fat_p::enforce`
- `fat_p::ScopeGuard`
- `fat_p::CheckedArithmetic` family

Core should be minimal and intentionally curated.

#### B.2 Adding a new root symbol requires process
If you want a new symbol in the root namespace:
1. It must be unique and not likely to collide with other modules.
2. It must be justified as a true cross‑cutting primitive.
3. It must be added to the **Include‑All TU** (Section 6) and proven composable.

**Default policy:** do not add new root symbols.

---

### Rule C — Every module owns its names
Every non-core component must live in a module namespace:

- `fat_p::json_stream`
- `fat_p::cbor_stream`
- `fat_p::stable_hash_map`
- `fat_p::slot_map`
- `fat_p::feature`
- etc.

#### C.1 Example: streaming parsers
✅ Good:
```cpp
namespace fat_p::json_stream {
  enum class ParseStatus { NeedMoreInput, Done, Error };
  struct ParseError { ... };
  class JsonStreamParser { ... };
}
```

❌ Bad:
```cpp
namespace fat_p {
  enum class ParseStatus { ... };   // collides across modules
  struct ParseError { ... };        // collides across modules
}
```

---

### Rule D — Headers must be include‑order independent
A public header must not compile only when included after some other header.

#### D.1 Common include‑order landmine
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

### Rule E — No duplicate entity definitions across headers
A symbol may only be defined in one header file if two headers can be included together.

This includes:
- functions (even `inline` functions),
- templates,
- helper structs in `fat_p::detail`,
- any named entity with external linkage.

#### E.1 Why “inline” is not a free pass
Even `inline` functions/templates cannot be defined twice in the **same translation unit**. If header A and header B both define `detail::foo()` and the user includes both, the compile fails.

#### E.2 Required fix pattern
If two modules need the same helper:
- create a single shared header (e.g., `CSRMatrixPartitioning.h`)
- put the helper there
- include that shared header from both modules

---

### Rule F — Macro hygiene (single source of truth)
All configuration macros must be:
- prefixed `FATP_`,
- defined in **one** place,
- guarded with `#ifndef` to prevent redefinition,
- and never redefined in multiple headers.

#### F.1 Required file
Create and use a central config header (example name):
- `FatPConfig.h`

#### F.2 Example: `FATP_NO_UNIQUE_ADDRESS`
✅ Good:
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

❌ Bad:
```cpp
// DebugOnly.h
#define FATP_NO_UNIQUE_ADDRESS ...

// FastHashMap.h
#define FATP_NO_UNIQUE_ADDRESS ...   // ❌ macro redefinition
```

---

### Rule G — Public headers must be self-contained
A public header must compile when included alone (in an otherwise empty TU):

```cpp
#include "ThatHeader.h"
int main() {}
```

This implies:
- it includes all required standard headers,
- it does not rely on transitive includes,
- it does not rely on macros from other headers (unless those macros are defined in the central config header).

---

### Rule H — No `using namespace` in headers
No public header may contain:
```cpp
using namespace fat_p;
using namespace std;
```

No exceptions.

---

### Rule I — Explicit ownership of “detail”
All internal-only helpers must live in:
- `fat_p::detail` (global internal helpers), or
- `fat_p::<module>::detail` (module-private helpers)

**Do not** put “detail” helpers in the root without a namespace (`static` functions at global scope are forbidden in public headers).

---

### Rule J — No backwards compatibility shims for hygiene fixes
When a systemic hygiene issue is fixed:
- do not leave alias macros,
- do not leave deprecated typedefs,
- do not ship old names “for compatibility.”

Fix the problem fully and update call sites/docs.

---

## 4. Preferred Patterns (SHOULD)

These are strong recommendations; exceptions require justification.

### 4.1 Prefer nested module namespaces over unique global names
Instead of inventing globally unique names like `FatPJsonStreamParseStatus`, use:
- `fat_p::json_stream::ParseStatus`

### 4.2 Prefer fully qualified internal calls
Inside headers, prefer:
```cpp
return ::fat_p::checked_add<Policy>(a, b);
```
over relying on unqualified lookup.

### 4.3 Prefer `inline constexpr` for constants
C++17 supports inline variables:
```cpp
inline constexpr std::size_t kDefaultLimit = 1024;
```

Avoid non-inline global variables in headers.

### 4.4 Prefer snapshot-before-callback in thread-safe code
If callbacks are invoked outside locks (recommended), snapshot the callback list under lock to avoid races and invalidation.

---

## 5. Required Examples (Bad vs Good)

### Example 1 — Flattening causes collisions (JSON stream vs CBOR stream)
**Bad:**
```cpp
// JsonStreamLite.h
namespace fat_p {
namespace json_stream { struct ParseError {}; }
using json_stream::ParseError; // ❌
}
```

```cpp
// CborStreamLite.h
namespace fat_p {
namespace cbor_stream { struct ParseError {}; }
using cbor_stream::ParseError; // ❌ redefinition
}
```

**Good:**
```cpp
namespace fat_p::json_stream { struct ParseError {}; }  // ✅
namespace fat_p::cbor_stream { struct ParseError {}; }  // ✅
```

---

### Example 2 — Include-order ambiguity (`checked_cast`)
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

### Example 3 — Duplicate helper defined in multiple headers
**Bad:**
```cpp
// CSRMatrixParallel.h defines detail::compute_balanced_partitions
// CSRMatrix_HPC.h defines detail::compute_balanced_partitions
// Include both -> redefinition ❌
```

**Good:**
```cpp
// CSRMatrixPartitioning.h defines compute_balanced_partitions once ✅
// Both headers include CSRMatrixPartitioning.h ✅
```

---

## 6. Enforcement (CI / Build Gates)

### 6.1 Include-All compile test (MUST)
Maintain a compile-only TU that includes **all public headers**:

- `test_include_all_headers.cpp`

This must compile on all supported toolchains.

### 6.2 Header self-contained tests (MUST)
For each public header `X.h`, there must be a compile-only test TU:

- `test_include_X.cpp`
```cpp
#include "X.h"
int main() {}
```

This prevents “accidentally relied on transitive include” bugs.

### 6.3 “Randomized include order” test (SHOULD)
Optionally generate a few include-all variants with shuffled include order. This can be as simple as 5 pre-generated permutations committed to the repo.

### 6.4 Warning cleanliness
Public headers should be warning-clean under:
- GCC/Clang: `-Wall -Wextra -Wpedantic`
- MSVC: `/W4`

If the project enforces `-Werror`, treat any warning as a hygiene regression.

---

## 7. Review Checklist (PR Gate)

A change is not “done” until this checklist is satisfied:

### Namespace & symbol safety
- [ ] No new `using …;` exported into `namespace fat_p` at namespace scope.
- [ ] No new generic root names that could collide (`ParseError`, `StreamError`, etc.).
- [ ] Module namespace used for module-owned symbols.
- [ ] No `using namespace` in headers.

### Include & macro hygiene
- [ ] Header compiles when included alone.
- [ ] No macro redefinitions (central config used).
- [ ] New macros are prefixed `FATP_` and guarded with `#ifndef`.

### ODR & duplication
- [ ] No new helper defined in multiple headers.
- [ ] Shared helper moved to a single shared header if used in >1 module.

### CI gates
- [ ] Include-all TU compiles.
- [ ] Header self-contained TU compiles.

### AI / artifact delivery (when changes are provided as downloads)
- [ ] Response includes a `Modified Files (N)` list with repo-relative paths.
- [ ] Download links include **only** those `N` modified files (no extras).

---

## 8. Migration Guidance (when violations are found)

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

This policy exists to keep that from happening as Fat‑P grows.
