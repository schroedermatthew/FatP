# C++ headers, namespaces, and linkage

Applies to public/internal headers, definitions, namespaces, macros, C++ modules,
and linkage-sensitive changes. Use the project's configured layout and the
mandatory header-guard rule in [STYLE.md](STYLE.md).

## Composability law

**If two headers cannot be included together in any order, the library is broken.**
A regression is P0 within a supported configuration: fix the owning header rather
than adding a consumer include-order workaround. Mutually exclusive backends must
be explicit configurations, never selected accidentally by include sequencing.

No root-namespace flattening of module-owned names. Public root names require
cross-cutting ownership justification and a collision/composition check. No
`using namespace` in authored headers, including hidden local directives.

No umbrella headers or consumer link targets that pull unrelated component
requirements into every user. A deliberately designated public facade requires
architecture ratification. An include-all verification TU or build target that
runs all tests is tooling, not a prohibited consumer umbrella.

## Self-contained and composable interfaces

- Each public header must supply the declarations it uses through its own includes
  or valid forward declarations. Check it without a precompiled header, unity-build
  context, or a test framework supplying the missing includes.
- A documented generated configuration header or platform prerequisite is allowed;
  an unexplained include-order requirement is not. Keep configuration consistent
  wherever a public definition is seen.
- Forward-declare only where an incomplete type is sufficient. Do not invent
  declarations for standard-library types or hide a required complete type behind
  a fragile forward declaration.
- Include the implementation's own corresponding header early enough to expose
  missing prerequisites, following any documented platform/configuration preamble.
- Use #pragma once as required by STYLE.md. Guards prevent repeated inclusion of one file;
  they do not reconcile duplicate entities defined in different files.
- Check supported public-header combinations. For mutually exclusive documented
  configurations, test each valid configuration instead of demanding an impossible
  combined build. Record such boundaries explicitly.

## Names and macros

- Keep library APIs in their designated namespaces. Do not put using-directives
  at global or public namespace scope in a header and thereby alter a consumer's
  unqualified lookup. Place intentional aliases at an owned interface.
- Give helpers a component-owned detail namespace or suitable local scope. A
  common detail namespace alone does not prevent two components defining the same
  helper name.
- Put implementation types used by only one .cpp file in an unnamed namespace
  unless they intentionally need external identity. Another file can reuse that
  local spelling without renaming every helper.
- Treat unnamed namespaces and namespace-scope static state in headers as an
  explicit per-translation-unit design, never an accidental way to hide a shared
  counter, registry, mutex, or cache. Separate instances change semantics.
- Prefix project macros, avoid reserved identifiers, and centralize shared feature
  detection. Keep conditional definitions compatible with the selected standard
  and platform. Restore temporary macro or packing state instead of leaking it.

## The one-definition rule

Keep an entity's definition in one canonical source location. Multiple definitions
of the same entity in one translation unit are not legalized by inline. Some
entities can be defined in multiple translation units under the language's ODR
conditions, including requirements on definitions and name lookup. A successful
link is not proof that those conditions hold.
[C++ working draft: ODR](https://eel.is/c++draft/basic.def.odr).

For conventional headers outside named modules, use this decision guide:

| Entity or intent | Normal placement and check |
|---|---|
| Non-template externally linked function | Declaration in a header; one definition in a source file, or a valid inline header definition |
| Function/class template | Definition reachable where implicit instantiation needs it, unless deliberate explicit-instantiation arrangements provide the required code |
| Full specialization | Assess its own ODR/linkage requirements; do not assume template rules automatically make an ordinary function specialization inline |
| Shared mutable variable | One source definition with an extern declaration, or a deliberate inline variable where the standard and ABI model support it |
| One shared constant identity | An appropriate external definition or inline constexpr variable in supported language modes |
| Per-translation-unit constant/state | Deliberate internal linkage with semantics reviewed for separate identities |

A constexpr function is implicitly inline. In C++17 and later, a constexpr static
data member is implicitly inline; an ordinary namespace-scope constexpr variable
is not made inline by constexpr alone and commonly has internal linkage. Choose
value and identity requirements explicitly.
[constexpr declarations](https://eel.is/c++draft/dcl.constexpr),
[linkage](https://eel.is/c++draft/basic.link).

Inline is not a command requiring call-site inlining. It also does not guarantee
one process-wide registry across arbitrary dynamic-library/plugin boundaries;
validate the actual export, loading, and ABI model. For functions defined inside
a class, account for named-module rules rather than assuming the conventional
header rule applies unchanged.
[Inline rules](https://eel.is/c++draft/dcl.inline).

## Small language examples

Complete C++20 header example; it should compile when included as a header under
that mode. It follows [the template style](STYLE.md). The example namespace is
illustrative and must not be imported as a project's actual namespace.

```cpp
#pragma once

#include <cstddef>

namespace guidelineexample
{
inline constexpr std::size_t kSharedLimit = 16;

constexpr std::size_t doubled(std::size_t value) noexcept
{
    return value * 2;
}

static_assert(doubled(4) == 8);
} // namespace guidelineexample
```

Complete negative C++20 translation unit; it must fail with a redefinition
diagnostic. This is what can happen when two independently protected headers each
define the same helper and a consumer includes both.

```cpp
namespace guidelineexample
{
inline int helper()
{
    return 1;
}
inline int helper()
{
    return 1;
}
} // namespace guidelineexample
```

## Composition and binary verification

Use independent source files to check shared identity and linkage where those
properties matter. Include-order tests and single-file compilation cannot replace
such checks; nor can link success replace inspection for ODR violations that do
not require a diagnostic.

For named modules or header units, record supported compiler/build arrangements,
exported interfaces, generated artifacts, and interaction with textual headers.
Do not treat module artifacts as portable across arbitrary toolchains. Follow
[Build and CI](BUILD_AND_CI.md) for consumer, installation, and configuration checks.

## Required hygiene gates and response

For the supported configuration, compile each public header alone and an include-all
TU without PCH/unity prerequisites, plus representative reversed/permuted orders.
Shared definitions get multi-TU link/runtime checks. Use warning-clean authored
builds and an installed/external consumer where distribution is promised.

When composition breaks: reproduce the minimal pair, establish symbol/dependency
ownership, remove flattening or duplicate definitions, repair the owning header,
update affected users/docs/tests, then run the regression and the wider gate.
Do not hide it with an umbrella, consumer prerequisite, or pre-release alias shim.

Macros use the profile prefix and one definition home. Config defaults may use
#ifndef guards; conflicting mandatory definitions must diagnose rather than
silently keep an incompatible value. Local implementation macros are undefined.
Inventory exported/config macros from source and document consumer-set controls;
do not maintain hand-copied counts. Include guards remain STYLE's #pragma once.
