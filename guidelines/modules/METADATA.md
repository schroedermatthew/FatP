# File metadata

**Authority:** [CORE](../CORE.md). Facts belong in [PROJECT_PROFILE](../PROJECT_PROFILE.md).

## Purpose

`EXAMPLE_META` is a **machine-readable metadata block** embedded in comments to make the project repository easier to index and analyze (including by automated tools and AI assistants). It supports:

- component indexing (header → tests → benchmarks → docs)
- faster navigation for large refactors and reviews
- hygiene visibility (macro surface, platform gates, include hygiene)
- CI linting for drift (moved files, missing links, inconsistent roles)

`EXAMPLE_META` must be **comment-only** and must never change compilation behavior.

## Applicability

Metadata adoption is explicit in [PROJECT_PROFILE](../PROJECT_PROFILE.md).
An unresolved metadata.enabled is not silently enabled or disabled. When true,
every authored file in its declared surface requires metadata. When false, record
why it is not adopted. The rules still bind any metadata block being edited.

The profile owns actual authored_globs, exclusions, file_roles and layers. Do not
repeat those facts here. Coverage includes owned C++ files or explicit justified
exclusions. Stale patterns and empty enabled coverage fail validation. JSON, YAML
workflows, generated/vendor material and non-code outputs may be excluded as
outside the adopted comment-compatible surface.

EXAMPLE prefixes, bracketed descriptions and specimen paths below are illustrative
forms, not installed files or real generator claims. Real blocks use the profile's
macro_prefix and actual paths. Complete valid fixtures accompany the package tests.

## Placement rules

### Header files

1. `#pragma once` is the **first line**.
2. `EXAMPLE_META` goes **immediately after** it.
3. Any Doxygen file header (`@file`, `@brief`) goes **after** `EXAMPLE_META`.
4. `EXAMPLE_META` appears before any includes.

**Single source of truth:** layer classification lives in `EXAMPLE_META.layer`. Do not duplicate it in the Doxygen header.

### Source files

1. Keep any existing file header comment (license, `@file`) first.
2. Place `EXAMPLE_META` immediately after it; otherwise at the top of the file before includes.

### Build files

1. If the file begins with a required first directive (e.g. `cmake_minimum_required(...)`), keep it first; `EXAMPLE_META` follows immediately.
2. Otherwise place it at the top, before any commands.

### Script files (`.py`, `.sh`, `.ps1`)

1. Keep any shebang first; `EXAMPLE_META` follows immediately, before any executable statements.

### Blank line separation (all file types)

Always leave **one empty line** after the close of the `EXAMPLE_META` block before the next content. For `/* ... */` blocks the empty line follows the closing `*/`; for line-comment blocks it follows the last metadata line. This ensures the parser cleanly terminates the metadata region.

## Format

The block begins with the sentinel line `EXAMPLE_META:` followed by YAML. The YAML content is identical across file types; only the comment wrapper changes.

### Block-comment form (C/C++)

```cpp
/*
EXAMPLE_META:
  meta_version: 1
  ...
*/
```

### Line-comment form (build files, Python, shell)

Each YAML line is prefixed with the file's line-comment marker plus one space:

```text
# EXAMPLE_META:
#   meta_version: 1
#   ...
```

Parsing rule for tooling: strip the comment marker and one following space from each line, then parse the remainder as YAML.

### Comment terminator safety (critical)

Inside a `/* ... */` block comment, the byte sequence **`*/` terminates the comment** even inside quotes or YAML strings.

**Therefore: `EXAMPLE_META` content must never contain `*/` anywhere.**

This specifically forbids glob patterns such as `docs/**/*Foo*`, `**/*`, `**/`, and any value where `*` is immediately followed by `/`. If you need a hint that would normally be a recursive glob, use **plain-text search fields** (`docs_search`) or explicit path lists instead. Violating this rule does not merely corrupt the metadata — it breaks compilation of the file.

### Formatting constraints

- **Indentation:** 2 spaces preferred.
- **Encoding:** ASCII or UTF-8.
- **Line length:** at most 120 columns.
- **Key order:** follow the canonical key order below to reduce merge conflicts.

## Schema v1

### Required keys (all files)

| Key | Type | Meaning |
|---|---|---|
| `meta_version` | int | Schema version. Must be `1`. |
| `component` | string or list | Canonical component name(s) for this file. |
| `file_role` | enum | Role of the file in the repository (project-defined; see below). |
| `path` | string | Repo-relative path, forward slashes. |
| `layer` | enum | Logical layer label (project-defined; see below). |
| `summary` | string | One sentence describing the file's purpose. No marketing. |

### Strongly recommended keys

| Key | Type | Meaning |
|---|---|---|
| `namespace` | string or list | Primary namespaces defined or used. |
| `api_stability` | enum | `in_work` / `experimental` / `candidate` / `stable`. |
| `related` | map | Links to docs/tests/benchmarks relevant to this file. |
| `hygiene` | map | Machine-derived signals; only recomputable keys may appear (see below). |
| `generated` | map | Marks the block as tool-managed. |

### `file_role` enumeration (project-defined)

The closed file_roles and layers lists live only in the project profile.
Do not inherit a six-layer taxonomy or mandate a Testing layer by name. Settle
unresolved classification rather than inventing a best-guess layer.

## Canonical key order

1. `meta_version`
2. `component`
3. `file_role`
4. `path`
5. `namespace`
6. `layer`
7. `summary`
8. `api_stability`
9. `related`
10. `hygiene`
11. `generated`

## `related` section

`related` connects evidence and explanations to the file. All paths are repo-relative.

```yaml
related:
  docs:
    - [path to component manual]
  tests:
    - [path to component test file]
  benchmarks:
    - [path to component benchmark]
```

Rules:

- Prefer explicit file paths when they exist; the lists are lists even at size 1.
- Entries must point to files that exist in-tree (CI-validated).
- For `in_work` components, plain-text search fields are allowed instead of paths:

```yaml
related:
  docs_search: "AsyncOperations"
  tests_search: "test_AsyncOperations"
```

Search fields must be plain text and must not contain `*/`.

## `hygiene` section

`hygiene` is **machine-derived. Do not hand-edit counts** — hand-edited counts will drift and then lie.

```yaml
hygiene:
  pragma_once: true
  include_guard: false
  defines_total: [computed by tooling]
  defines_unprefixed: [computed by tooling]
  undefs_total: [computed by tooling]
```

Rules:

- `defines_total` counts `#define` occurrences in the file.
- `defines_unprefixed` counts `#define`s that do not start with `EXAMPLE_` and are not `#undef`'d.
- If a value is unknown or not computed, omit it — do not guess.
- The five keys above are the ones the supplied checker recomputes from the file.
  It rejects any hygiene key it cannot recompute, so a value it does not derive
  cannot be asserted here. `includes_platform_header` is the concrete case: platform
  classification depends on the project's own platform-header list, so record it
  only after a project-specific checker computes it, and expect the supplied gate
  to refuse it until then.

**Macro prefix requirement:** macros follow the prefix rule in `../cpp/HEADERS_AND_LINKAGE.md`; `undefs_total` counts the cleanups.

## `generated` section

To prevent manual drift, mark tool-managed blocks:

```yaml
generated:
  by: [tool name]
  mode: autogen
```

A metadata update tool must overwrite only the `EXAMPLE_META` region and leave the rest of the file unchanged. If a file's metadata is intentionally hand-maintained, omit `generated` and keep the block minimal.

## Component naming rules

- `component` uses the canonical component name (usually the public header stem).
- A file spanning multiple components (rare) uses a list.
- Tests and benchmarks name the primary component under test, not the test harness.

## Contract rules

`EXAMPLE_META` is an index layer, **not the contract itself**.

- Do not restate full invariants in the block.
- Invariants live in the manual or clearly labeled internal-invariants sections.
- Use `related.docs` to point to the canonical invariant location.

## Update rules

Update the block when: the file moves (`path`), the component association changes (`component`), or tests/benchmarks/docs move or are replaced (`related.*`). PRs changing public APIs should update `summary` if responsibility changed and `related.*` if evidence moved.

Never hand-edit: `hygiene` counts and `generated` fields.

## Supplied enforcement

Run `python tools/lint_metadata.py --repo-root . --guidelines guidelines`.
It validates coverage, safe YAML parsing, duplicate/alias rejection, required keys
and order, comment-only placement, version/prefix, actual paths, closed enums,
related paths, blank separation and known hygiene counts. Unknown/uncomputed
platform-header classifications are omitted or checked by a real project tool.

No metadata writer or regeneration tool is supplied. Review generated-tool
provenance and run the actual generator's check if using it. See [tool coverage](../../tools/README.md).
Missing dependencies, unsupported wrappers and malformed configuration fail; no
metadata check proves the indexed component's correctness or architecture.

## Examples

### Public header

```cpp
#pragma once
/*
EXAMPLE_META:
  meta_version: 1
  component: AlignedVector
  file_role: public_header
  path: include/example/AlignedVector.h
  namespace: example
  layer: [layer value]
  summary: Cache-aligned contiguous storage vector.
  api_stability: candidate
  related:
    docs:
      - [docs path]
    tests:
      - [tests path]
  hygiene:
    pragma_once: true
    defines_total: 0
    defines_unprefixed: 0
    undefs_total: 0
*/
```

### In-work component (safe hinting, no globs)

```cpp
#pragma once
/*
EXAMPLE_META:
  meta_version: 1
  component: AsyncOperations
  file_role: public_header
  path: include/example/AsyncOperations.h
  namespace: example
  layer: [layer value]
  summary: Public header for AsyncOperations.
  api_stability: in_work
  related:
    docs_search: "AsyncOperations"
  hygiene:
    pragma_once: true
    defines_total: 0
    defines_unprefixed: 0
    undefs_total: 0
*/
```

### Test file

```cpp
/**
 * @file test_AlignedVector.cpp
 * @brief Unit tests for AlignedVector.h
 */
/*
EXAMPLE_META:
  meta_version: 1
  component: AlignedVector
  file_role: test
  path: tests/test_AlignedVector.cpp
  namespace: example::testing::alignedvector
  layer: [declared layer]
  summary: Unit tests for AlignedVector.
  api_stability: in_work
  related:
    headers:
      - include/example/AlignedVector.h
  hygiene:
    pragma_once: false
    include_guard: false
    defines_total: 0
    defines_unprefixed: 0
    undefs_total: 0
*/
```

## Common mistakes

- Placing `EXAMPLE_META` after includes (harder to discover reliably).
- Hand-editing machine-derived counts (they will drift).
- Using unstable or invented component names.
- Treating the block as a substitute for manuals and invariants.
- **Embedding `*/` inside values** (terminates the comment and breaks compilation).
- Omitting the blank line after the block.
- Inheriting another project's `applies_to`, layer names, or tooling claims without checking they describe this repository.

## Versioning

Schema changes bump `meta_version` and must ship with: updated guidelines, updated lint expectations, and a repo-wide metadata regeneration pass.
