# C++ naming and style

These are mandatory C++ rules for authored code and documentation examples.
They are not defaults, recommendations, or project-profile choices. Instantiating
the template does not reopen them. **Google C++ Style Guide and the Google
formatter preset are prohibited.** Do not substitute another dialect because an
editor, external guide, existing sample, or assistant prefers it.

The project profile cannot waive this guide. Changing a convention
requires an explicit owner instruction authorizing that specific change to the
guide; ordinary implementation, refactoring, and adoption instructions do not.
Record any such ruling in the canonical guide and update the formatter and examples
together. Do not create a standing local-exception form or an assistant-selected opt-out.

## Naming

| Element | Convention | Examples |
|---|---|---|
| Classes, structs, enums, project type aliases | PascalCase | SampleBuffer, SampleInfo, BufferPolicy |
| Functions and methods | camelCase | findEntry, insertOrAssign, isValid |
| Parameters and local variables | camelCase | entryIndex, sampleCount, hashValue |
| Class instance data members | m plus PascalCase | mCapacity, mLoadFactor, mFrozen |
| Plain aggregate data members | camelCase, no prefix | id, priority, sampleCount |
| Mutable or runtime-initialized static data members | s plus PascalCase | sInstanceCount, sDefaultPolicy |
| Named constexpr constants | k plus PascalCase | kDefaultCapacity, kMaxRetries |
| Template type and value parameters | PascalCase | Key, Value, Policy, Capacity |
| Standard-library protocol aliases | Required standard spelling | value_type, size_type, iterator_category |
| Macros | Project-prefixed SCREAMING_SNAKE | PROJECT_FEATURE_ENABLED |
| Namespaces | Lowercase, with nested ownership scopes | project, project::detail |

Use the m prefix for invariant-bearing types, including a struct with private
state or user-declared construction/destruction. Plain aggregate records with
public data use unprefixed fields. The choice follows the type's role, not merely
whether the declaration says class or struct. Do not introduce member_name,
m_memberName, or memberName_ as competing member conventions.

The k convention takes precedence for a static constexpr constant; s identifies
other static data members. A compile-time template argument uses Capacity, not
kCapacity. Name enum values with PascalCase unless implementing an external
interface whose spelling is fixed.

Preserve actual standard-library interface names: push_back, emplace_back,
pop_front, begin, end, size, empty, and their required aliases. This exception
serves generic interoperability; it does not make unrelated methods snake_case.
Use isValid and computeHash, not is_valid and compute_hash.

Use descriptive names tied to the operation and domain. Avoid serial variants
such as insert2 and ambiguous abbreviations where a clear name is practical.
Keep externally mandated API, generated, and vendor names intact at their boundary.
Public header/source pairs use TypeName.h and TypeName.cpp; test filenames
follow the receiving project's registered test-discovery pattern.

## Semantic names

Names describe the invariant or intent, not an incidental implementation algorithm.
Avoid serial variants and unexplained abbreviations. Type template parameters use
PascalCase without artificial T suffix/prefix (Key, not KeyT or TKey); T itself is
valid for an intentionally generic type. This semantic check remains part of review.

| Adjective | Binding meaning |
|---|---|
| Checked | Documented access-time validation enabled; structural checks always remain active |
| Unchecked | Only documented optional access-time validation omitted; never invalid construction |
| Default | The explicitly documented default check policy; identify build-dependent behavior |
| Safe | Name the actual bounds/lifetime/type guarantees and limits |
| ThreadSafe | State permitted concurrent operations and the synchronization/ownership strategy |

No vague Fast/Smart/Simple type adjectives. Define any additional adjective in the
profile's additional decisions before using it. Existing meanings cannot be removed
or redefined without owner direction. A lexical naming pass does not verify this table.

## Formatting

The canonical formatter configuration is [the supplied .clang-format](../../.clang-format),
copied to the receiving repository root. The link above is rewritten on adoption
when guidelines live under docs/guidelines.
It starts from LLVM's formatter baseline and explicitly sets this template's
conventions. LLVM is the formatter baseline, not an imported naming guide.
Google style is not an allowed alternative or fallback.

- Use four spaces; never tabs. Do not add an indentation level inside namespaces.
- Use Allman braces: block-opening braces go on their own line. Always brace
  control-statement bodies, including single statements. Do not collapse functions,
  loops, or conditionals onto one line; empty lambdas are the configured exception.
- Align pointer and reference markers with the type: int* pointer and const Item& item.
  Use a space before control-statement parentheses, but not before a function call.
- When parameters or arguments wrap, put one per line rather than packing several
  onto each continuation line. Put a template declaration above its declaration.
- Put each constructor initializer on its own line when present, with leading
  commas after the first initializer. Keep access labels aligned with the class's
  opening/closing braces.
- Aim for 100 columns. Lines from 101 through 120 columns are compliant; flag them
  only when readability suffers. The hard limit is 120, except macro-definition
  lines, including their continued definition lines.
- Retain useful blank-line groups and namespace closing comments. Do not align
  unrelated declarations or assignments into wide padding tables.
- Use #pragma once in authored headers. Do not add a second include-guard scheme.
  If a required toolchain cannot support it, report the concrete conflict to the
  owner; do not silently replace the guard policy in the project profile.

ColumnLimit is 120. PenaltyExcessCharacter applies beyond that limit, not beyond
100, so the 100-column preference still requires judgment. The formatter cannot
guarantee every hard-width or brace rule, especially in macros and preprocessor
regions. Review brace insertions because clang-format lacks full semantic context.
[Formatter option semantics](https://clang.llvm.org/docs/ClangFormatStyleOptions.html#penaltyexcesscharacter).

## Include groups

Use blank-line-separated groups, case-insensitive alphabetical order inside each
group, and no inherited application layer names. A corresponding component header is the first include in component source and
test files, in its own group; a genuinely required preamble is a documented
architecture boundary, not an accidental transitive prerequisite. After it, group standard-library headers first,
external dependencies next, and project headers by the project's dependency
layers. Test-only support belongs after production dependencies.

The formatter uses SortIncludes: Never and preserves authored ordering. It does
not infer architectural layers or enforce alphabetical order. The review checks
those groups; the style tool checks explicitly mapped component-header-first files. Include spelling and layer names remain local decisions.
Fix ordering when touching the relevant includes; avoid unrelated reorder-only
changes. Header self-containment still follows [Headers and linkage](HEADERS_AND_LINKAGE.md).

## Complete compile-only style example

This C++20 translation unit illustrates names, member distinctions, and layout.
It is not a production buffer implementation or a requirement to use an atomic.

```cpp
#include <atomic>
#include <cstddef>

namespace guidelineexample
{
struct SampleInfo
{
    std::size_t sampleCount = 0;
    bool enabled = false;
};

class SampleBuffer
{
public:
    using value_type = double;
    static constexpr std::size_t kDefaultCapacity = 16;

    explicit SampleBuffer(std::size_t capacity = kDefaultCapacity)
        : mCapacity(capacity)
    {
    }

    std::size_t capacity() const noexcept
    {
        return mCapacity;
    }

    static std::size_t nextIdentifier() noexcept
    {
        return ++sNextIdentifier;
    }

private:
    std::size_t mCapacity;
    inline static std::atomic<std::size_t> sNextIdentifier{0};
};
} // namespace guidelineexample
```

## Enforcement and adoption

Copy the supplied formatter file with the guidelines; do not let editor or CI
fallback select another preset. Use file-based style selection and record a
compatible formatter version in the project profile. The reference configuration is
validated with clang-format 22.1.8; this is a formatter version, not a C++ standard
or compiler requirement. Revalidate after changing formatter versions.

clang-format controls layout, not identifier naming, ownership,
or architectural conformance. Run the supplied style checker and review the remaining semantic obligations
listed in [tool coverage](../../tools/README.md). Its Clang AST pass checks the
m/s/k distinction, records vs classes, and lexical naming in selected files.
Protocol spelling permission still needs evidence of actual interoperability. Do not report all style
rules enforced just because the formatter check passed.

Before delivering authored C++ changes, check the names, member distinctions,
braces, indentation, width, and header guards against this guide. A violation is
a conformance defect, not a reviewer's optional aesthetic suggestion. Correct
violations introduced by the change before calling it complete; record assistant
violations under [D03 in the mandatory ledger](../DEMERITS.md).

Externally mandated names, standard-library protocols, generated files, and vendor
code retain their required spelling or ownership boundary. Those boundaries do
not allow unrelated authored code to adopt an external style. Where existing
authored code conflicts, identify the affected scope and follow the owner's
direction; adoption alone does not authorize unrelated repository-wide reformatting
or breaking API renames, and existing drift does not waive this guide for new work.

## Verification command and coverage

Run `python tools/check_style.py --repo-root . --guidelines guidelines --file PATH`
from the receiving root, supplying actual include/define options after `--`.
Use `--inventory` to check the profile's authored C++ inventory. The checker never
rewrites source. Missing tools or parse errors fail; they are not style passes.
Header spellings and template-parameter semantics have manual checks as described
in the tool coverage. Correctness, justified protocol exceptions, useful names,
macro semantics and architectural layering always require review.

The complete compile-only example above is tested as C++20; GUIDELINE/EXAMPLE
names in documentation are synthetic examples, not receiving-project identities.
