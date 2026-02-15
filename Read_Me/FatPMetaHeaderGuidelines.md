---
doc_id: readme-fatp-meta-header-guidelines
doc_type: governance
status: active
audience: contributors
applies_to:
  - include/fat_p/**/*.{h,hpp,inl}
  - components/**/*.{h,hpp,inl,cpp,cc,cxx}
  - cmake/**/*.{cmake}
  - CMakeLists.txt
  - tools/**/*.{py,sh,ps1}
  - tooling/**/*.{py,sh,ps1}
version: 2
last_updated: 2026-02-14
---

# FAT-P Meta Header Guidelines

## Purpose

`FATP_META` is a **machine-readable metadata block** embedded in C/C++ comments to make the FAT-P project easier to index and analyze (including by automated tools and AI assistants). It supports:

- component indexing (header → tests → benchmarks → docs)
- faster navigation for large refactors and reviews
- hygiene visibility (macro surface, platform gates, include hygiene)
- CI linting for drift (moved files, missing links, inconsistent roles)

`FATP_META` must be **comment-only** and must not change compilation behavior.

## Applicability

`FATP_META` is required for every **repository-authored code file** in the paths listed in the front-matter `applies_to:` block.

Exclusions (no `FATP_META` required):

- YAML files (`.yml`, `.yaml`) including `.github/workflows/*`
- Vendored code under `ThirdParty/`
- Generated directories (for example `.vcpkg_installed/`, `build/`)
- Non-code artifacts (for example `results/` outputs)

**Rationale:** Requiring metadata on the code surface enables deterministic indexing (component ownership, layer classification, and cross-links) and allows CI to detect drift when files move.

## Placement rules

### Header files (`.h`)

**Required header layout:**

1. `#pragma once` must be the **first line** of the file.
2. Place `FATP_META` **immediately after `#pragma once`**.
3. If the file has a Doxygen file header (`@file`, `@brief`, etc.), place it **after `FATP_META`**.
4. `FATP_META` must appear before any includes.

**Single source of truth:** Architectural layer classification lives in `FATP_META.layer`.
Do **not** duplicate it in the Doxygen file header (no `@layer` tag).

Legacy compatibility: headers that still use include guards follow the same ordering: include guard, then `FATP_META`, then optional Doxygen file header, then includes.

### Source files (`.cpp`)

1. If there is an existing file header comment (license, `@file`, etc.), keep it first.
2. Place `FATP_META` immediately after the existing header comment block.
3. Otherwise, place it at the top of the file before includes.


### CMake files (`CMakeLists.txt`, `.cmake`)

1. If the file begins with a `cmake_minimum_required(...)` line, keep it first.
2. Place `FATP_META` immediately after that line.
3. Otherwise, place `FATP_META` at the top of the file before any CMake commands.

### Script files (`.py`, `.sh`, `.ps1`)

1. If the file has a shebang line (`#!...`), keep it first.
2. Place `FATP_META` immediately after the shebang (or at the top if there is no shebang).
3. Place `FATP_META` before any executable statements.

### Blank line separation (all file types)

Always leave **one empty line** after the closing of a `FATP_META` block before the next content (includes, code, or other comments). This improves readability and ensures the parser cleanly terminates the metadata region.

For `/* ... */` blocks, the empty line follows the closing `*/`. For line-comment blocks (`//` or `#`), the empty line follows the last metadata line.

## Format

`FATP_META` is a comment block that begins with the sentinel line `FATP_META:` followed by YAML. The YAML content MUST be identical across file types; only the comment wrapper changes.

### C/C++ block comment form

```cpp
/*
FATP_META:
  meta_version: 1
  ...
*/
```

### Line-comment form (CMake, Python, shell, PowerShell)

Each YAML line is prefixed with the file’s line comment marker plus a single space.

```text
# FATP_META:
#   meta_version: 1
#   ...
```

Parsing rule for tooling: remove the comment marker and one following space from each `FATP_META` line, then parse the remaining text as YAML.

### Comment terminator safety (critical)

Inside a `/* ... */` block comment, the byte sequence **`*/` terminates the comment** even if it appears inside quotes or YAML strings.

**Therefore: `FATP_META` content must never contain `*/` anywhere.**

This specifically forbids glob patterns such as:

- `Documentation/**/*AsyncOperations*`
- `**/*`
- `**/`
- any value containing a `*` immediately followed by `/`

If you need a “hint” that would normally be written as a recursive glob, use **search strings** (`docs_search`) or explicit lists (`related.docs`). Do not embed glob syntax that can form `*/`.

### Formatting constraints

- **Indentation:** 2 spaces preferred. The parser tolerates tabs (converted to 2 spaces) and small indentation errors on top-level keys.
- **Encoding:** ASCII or UTF-8.
- **Line length:** ≤ 100 columns.
- **Key order:** follow the canonical key order below to reduce merge conflicts.

## Schema v1

### Required keys (all files)

| Key | Type | Meaning |
|---|---|---|
| `meta_version` | int | Schema version. Must be `1`. |
| `component` | string or list[string] | Canonical component name(s) associated with this file. |
| `file_role` | enum | Role of the file in the repository (see enums). |
| `path` | string | Repo-relative path using forward slashes. |
| `summary` | string | One sentence describing the file’s purpose. Avoid marketing. |

### Required keys (all files, continued)

| Key | Type | Meaning |
|---|---|---|
| `layer` | string | Logical layer label. For `public_header` and `internal_header` roles, use the component's architectural layer (`Foundation`, `Containers`, `Concurrency`, `Domain`, `Integration`). For `test`, `benchmark`, `tooling`, and `doc_support` roles, use `Testing`. Required for all file roles. |

### Strongly recommended keys

| Key | Type | Meaning |
|---|---|---|
| `namespace` | string or list[string] | Primary namespaces defined or used (`fat_p`, `fat_p::detail`, …). |
| `api_stability` | enum | Stability classification for the public surface. |
| `related` | map | Links to docs/tests/benchmarks relevant to this file. |
| `hygiene` | map | Machine-derived signals (macro counts, platform includes). |
| `generated` | map | Indicates the block is tool-managed. |

### `file_role` enum

- `public_header`
- `internal_header`
- `source`          # non-test, non-benchmark translation unit
- `test`
- `benchmark`
- `doc_support`
- `build_script`    # CMake and build glue authored in-repo
- `tooling`

### `api_stability` enum

- `in_work`
- `experimental`
- `candidate`
- `stable`

## Canonical key order

Use this order inside `FATP_META`:

1. `meta_version`
2. `component`
3. `file_role`
4. `path`
5. `namespace`
6. `layer` *(architectural layer for headers; `Testing` for test/benchmark/tooling)*
7. `summary`
8. `api_stability`
9. `related`
10. `hygiene`
11. `generated`

## `related` section

`related` connects evidence and explanations to the file. All paths are repo-relative.

Recommended structure:

```yaml
related:
  docs:
    - components/<Component>/docs/<Component>_User_Manual.md
  tests:
    - components/<Component>/tests/test_<Component>.cpp
  benchmarks:
    - components/<Component>/benchmarks/benchmark_<Component>.cpp
```

For components that are still `in_work`, you may include **plain-text search fields** instead of glob patterns:

```yaml
related:
  docs_search: "AsyncOperations"
  tests_search: "test_AsyncOperations"
```

Rules:

- Prefer explicit file paths (`docs/tests/benchmarks`) when they exist.
- `docs/tests/benchmarks` are lists (even if size 1).
- Entries must point to files that exist in-tree (CI-validated).
- Search fields must be **plain text** and must not contain `*/`.

## `hygiene` section

`hygiene` is intended to be **machine-derived**. Do not hand-edit counts.

Recommended structure:

```yaml
hygiene:
  pragma_once: true
  include_guard: false
  defines_total: 3
  defines_unprefixed: 0
  undefs_total: 1
  includes_windows_h: false
```

Rules:

- `defines_total` counts `#define` occurrences in the file.
- `defines_unprefixed` counts `#define` occurrences that do **not** start with `FATP_`.
- `includes_windows_h` is true if `<windows.h>` is included directly.
- If a value is unknown or not computed, omit it (do not guess).

**Macro prefix requirements:**

The `FATP_` prefix is required for **all** macros in **all** Fat-P source files, including tests and benchmarks. This prevents collisions when files are compiled together in a single translation unit (unity builds, `IncludeAllFatPHeaders.h`, combined test runners).

| Approach | Example | Compliant |
|----------|---------|-----------|
| Prefixed macro | `#define FATP_HAS_BOOST 0` | ✅ Yes |
| Unprefixed + `#undef` | `#define HAS_BOOST 0` ... `#undef HAS_BOOST` | ✅ Yes |
| Unprefixed, no cleanup | `#define HAS_BOOST 0` | ❌ No |

If an unprefixed macro is necessary for readability or third-party compatibility, it **must** be `#undef`'d before end of file. The `undefs_total` count should match the number of such cleanups.

## `generated` section

To prevent manual drift, mark the block as tool-managed:

```yaml
generated:
  by: fatp-meta-tool
  mode: autogen
```

Rules:

- A metadata update tool must overwrite only the `FATP_META` region and leave the rest of the file unchanged.
- If a file’s metadata is intentionally hand-maintained, omit `generated` and keep the block minimal.

## Component naming rules

- `component` must use the canonical component name (usually the public header stem):
  - `StableHashMap`, `SmallVector`, `AlignedVector`, `Expected`, `JsonLite`, etc.
- If a file spans multiple components (rare), use a list:
  - `component: [ConcurrencyPolicies, ThreadPool]`
- Tests and benchmarks should name the primary component under test (not the harness):
  - `component: SmallVector` (not `FatPTest`)

## Contract rules

`FATP_META` is an index layer, not the contract itself.

- Do not restate full invariants in `FATP_META`.
- Put invariants in the manual or in clearly labeled “Internal Invariants” sections.
- Use `related.docs` to point to the canonical invariant location(s).

## Update rules

### When you must update `FATP_META`

- File moved/renamed → update `path`.
- Component association changed → update `component`.
- Tests/benchmarks/docs moved or replaced → update `related.*`.

### What must not be edited by hand

- `hygiene` counts and derived flags.
- `generated` fields.

### Review expectations

PRs that change public APIs should update:

- `summary` if behavior or responsibility changed
- `related.docs` if manuals are affected
- `related.tests/benchmarks` if evidence moved or new files were added

## CI enforcement

Add a CI “meta lint” step with these checks:

1. **Presence**
   - Required files have a `FATP_META` block.

2. **Parse**
   - YAML parses.
   - `meta_version == 1`.
   - Required keys present.

3. **Consistency**
   - `path` matches the file location.
   - `file_role` matches extension and directory (`components/*/tests/` → `test`, `components/*/benchmarks/` → `benchmark`).
   - `related.*` targets exist when present.

4. **No drift**
   - If `generated.mode == autogen`, tool recomputation matches any present `hygiene` values.

## Examples

### Public header example

```cpp
#pragma once
/*
FATP_META:
  meta_version: 1
  component: AlignedVector
  file_role: public_header
  path: include/fat_p/AlignedVector.h
  namespace: fat_p
  layer: Containers
  summary: Cache-aligned contiguous storage vector.
  api_stability: candidate
  related:
    docs:
      - components/AlignedVector/docs/AlignedVector_User_Manual.md
    tests:
      - components/AlignedVector/tests/test_AlignedVector.cpp
    benchmarks:
      - components/AlignedVector/benchmarks/benchmark_AlignedVector.cpp
  hygiene:
    pragma_once: true
    defines_total: 0
    defines_unprefixed: 0
    undefs_total: 0
    includes_windows_h: false
  generated:
    by: fatp-meta-tool
    mode: autogen
*/
```

### In-work component example (safe hinting)

```cpp
#pragma once
/*
FATP_META:
  meta_version: 1
  component: AsyncOperations
  file_role: public_header
  path: include/fat_p/AsyncOperations.h
  namespace: fat_p
  summary: Public header for AsyncOperations.
  api_stability: in_work
  related:
    docs_search: "AsyncOperations"
    tests:
      - components/AsyncOperations/tests/test_AsyncOperations.cpp
  hygiene:
    pragma_once: true
    defines_total: 0
    defines_unprefixed: 0
    undefs_total: 0
    includes_windows_h: false
  generated:
    by: fatp-meta-tool
    mode: autogen
*/
```

### Test file example

Test and benchmark files use `layer: Testing` and must still use `FATP_` prefixed macros (or `#undef` unprefixed ones):

```cpp
/**
 * @file test_AlignedVector.cpp
 * @brief Unit tests for AlignedVector.h
 */
/*
FATP_META:
  meta_version: 1
  component: AlignedVector
  file_role: test
  path: components/AlignedVector/tests/test_AlignedVector.cpp
  namespace: fat_p::testing::alignedvector
  layer: Testing
  summary: Unit tests for AlignedVector.
  api_stability: in_work
  related:
    headers:
      - include/fat_p/AlignedVector.h
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
```


Note: All test and benchmark files use `layer: Testing` regardless of the component's own architectural layer. Macros should use `FATP_` prefix (`defines_unprefixed: 0`) to prevent collisions in unity builds.

## Tooling

The parser and validation scripts live at:

```
tools/fatp_meta_parser.py    # Parse and validate FATP_META blocks
tools/validate_layers.py     # Validate layer dependency hierarchy
```

**FATP_META Parser:**

```bash
# Validate all headers
python tools/fatp_meta_parser.py --validate include/fat_p/

# Dump parsed metadata as JSON
python tools/fatp_meta_parser.py --dump include/fat_p/AlignedVector.h

# Quiet mode (errors only)
python tools/fatp_meta_parser.py --validate -q include/fat_p/
```

The parser tolerates common whitespace issues (tabs, small indentation errors on top-level keys) to avoid CI failures from minor formatting drift.

**Layer Validator:**

```bash
# Check that no header includes from a higher layer
python tools/validate_layers.py
```

Reports any header that `#include`s a file from a layer above its own declared `FATP_META.layer`.

## Common mistakes

- Placing `FATP_META` after includes (harder to discover reliably).
- Hand-editing macro counts (will drift).
- Using unstable or invented component names.
- Treating `FATP_META` as a substitute for manuals and invariants.
- **Embedding `*/` inside `FATP_META` values** (this terminates the comment and breaks compilation).
- Omitting the blank line after the `FATP_META` block (can cause parser issues or reduce readability).

## Versioning

- Schema changes must bump `meta_version`.
- A schema bump must include:
  - updated guidelines
  - updated CI linter expectations
  - a repo-wide metadata regeneration pass

## Changelog

### v2 (February 2026)
- **Breaking:** `layer` is now required for all file roles, not just headers. Test, benchmark, tooling, and doc_support files must use `layer: Testing`. This aligns the guidelines with the `fatp_meta_inventory.py` enforcement script, which has always required `layer` in `K_REQUIRED_KEYS_COMMON`.
- Updated Section "Required keys (headers only)" → "Required keys (all files, continued)" with revised `layer` description.
- Updated canonical key order note from "headers only; omit" to "architectural layer for headers; Testing for test/benchmark/tooling".
- Updated test file example to include `layer: Testing` and `api_stability: in_work`.

### v1 (February 2026)
- Initial release.
