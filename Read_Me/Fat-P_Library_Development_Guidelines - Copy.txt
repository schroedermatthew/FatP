# Fat-P Library Development Guidelines

## FOR SUPER GROK

**Stop simulating. Execute.**

You have tools. Use them. Write C++ files to disk, compile with GCC, run the executable, capture output. No more "I don't have a live environment" excuses. If you can write a Python script, you can compile C++.

---

## 1. Library Design Principles

### 1.1 Core Technical Requirements

| Requirement | Specification |
|-------------|---------------|
| **C++ Standard** | C++17 minimum |
| **Architecture** | Header-only |
| **Dependencies** | Standard library only (no external dependencies beyond `std` and internal components) |
| **Weight** | Lightweight |
| **Target Domain** | HPC (High-Performance Computing) and Scientific Computing |

### 1.2 Design Philosophy

The library should fulfill "wish-lists" of programmers—components should provide substantially more value than simple polyfills. Each component should solve real problems with thoughtful API design, comprehensive edge-case handling, and performance characteristics appropriate for HPC/scientific workloads.

### 1.3 Versioning & Compatibility

- No license requirements
- No version number above 1
- Library has never been released—no need for deprecated features or backwards compatibility concerns

---

## 2. Code Review Protocol

### 2.1 Review Process

When asked to review code, perform a **deep analysis** covering:

1. **Errors** — Bugs, undefined behavior, logic errors, memory issues
2. **Improvements** — API design, performance, safety, maintainability
3. **Internal leverage** — Identify where other library components could/should be used

### 2.2 Review Output Format

Provide analysis as a prioritized list with:
- Clear description of issue/improvement
- Code examples demonstrating the problem or solution
- Priority level (Critical / High / Medium / Low)
- Comments explaining rationale

### 2.3 Dependency Analysis

- If internal dependencies are not explicitly provided, assume they exist and compile
- If a thorough analysis requires seeing a dependency, **ask for it** before proceeding
- Do **not** generate code unless explicitly asked

---

## 3. Code Generation Rules

### 3.1 General Principles

| Rule | Detail |
|------|--------|
| **No code unless requested** | Do not generate code unless explicitly asked |
| **No explanatory files** | Do not generate `.md`, `.txt`, or explanation files unless requested |
| **Preserve naming** | Never change file names or internal class names when modifying components |
| **Complete code only** | **NEVER provide truncated code**—always provide the entire file |
| **No AI comments** | Never include comments like `NEW`, `FIXED`, `BUGFIX`, `CHANGED`—comments describe *what* the code does, not the editing process |
| **Always compile** | Compile code before delivering it |
| **Provide download links** | If files are modified, always provide download links |

### 3.2 Formatting Standards

**Line width:** 100 columns maximum

**Style configuration (clang-format):**

```yaml
BasedOnStyle: LLVM
UseTab: Never
IndentWidth: 4
TabWidth: 4
InsertBraces: true
BreakBeforeBraces: Allman
AllowShortLoopsOnASingleLine: false
AllowShortIfStatementsOnASingleLine: false
AllowShortFunctionsOnASingleLine: None
AllowAllParametersOfDeclarationOnNextLine: false
BinPackParameters: false
BinPackArguments: false
BreakConstructorInitializers: BeforeComma
ConstructorInitializerIndentWidth: 4
IndentCaseLabels: true
ColumnLimit: 100
AccessModifierOffset: -4
NamespaceIndentation: None
FixNamespaceComments: true
SortIncludes: CaseInsensitive
SpacesInLineCommentPrefix:
  Minimum: 1
  Maximum: 1
```

### 3.3 Prohibited Content

- No special symbols or unusual Unicode characters in code
- No `using namespace` at **global scope** in examples (local scope is fine for brevity)
- No fictional macros—only document what exists in code

### 3.4 Using Directives Policy

| Scope | Allowed | Example |
|-------|---------|---------|
| Global scope | **NO** | `using namespace fat_p::diagnostic;` at file level |
| Function/block scope | **YES** | `void foo() { using namespace fat_p::diagnostic; ... }` |
| Namespace alias | **YES** | `namespace diag = fat_p::diagnostic;` |

Local `using` directives improve readability in examples without polluting the global namespace.

---

## 4. Unit Testing Standards

### 4.1 Testing Philosophy

- **Thorough testing** with 100% coverage goal
- Consider all corner cases and edge conditions
- Tests should validate both happy paths and failure modes

### 4.2 Testing Pattern (Reference Files)

Study these files for the canonical pattern:
- `test_BitSet.h` — Header pattern
- `test_BitSet.cpp` — Implementation pattern  
- `FatPTest.h` — Framework and assertions

### 4.3 Test File Structure

**Header file (`test_Component.h`):**
```cpp
#pragma once

#ifndef ENABLE_TEST_APPLICATION
namespace fat_p::testing
{

bool test_Component();

} // namespace fat_p::testing
#endif // #ifndef ENABLE_TEST_APPLICATION
```

**Implementation file (`test_Component.cpp`):**

Uses nested namespace to avoid linker collisions when multiple test files are linked together.

```cpp
#include <iostream>

#include "Component.h"
#include "FatPTest.h"

#ifndef ENABLE_TEST_APPLICATION
#include "test_Component.h"
#endif

namespace fat_p::testing::component
{

// All test functions go in the nested namespace
TEST_CASE(feature_one)
{
    // Test implementation using SIMPLE_ASSERT, ASSERT_EQ, etc.
    return true;
}

TEST_CASE(feature_two)
{
    // ...
    return true;
}

void benchmark_component()
{
    std::cout << "\n" << colors::cyan() << "Component Benchmarks:" 
              << colors::reset() << "\n\n";
    
    double time = measure_perf([&]() {
        // Operation to benchmark
        DoNotOptimize(result);
    }, iterations, warmup);
    
    std::cout << "Operation: " << format_time(time) << "\n";
}

} // namespace fat_p::testing::component

namespace fat_p::testing
{

bool test_Component()
{
    PRINT_HEADER(COMPONENT NAME)
    
    TestRunner runner;
    
    RUN_TEST_NS(runner, component, feature_one);
    RUN_TEST_NS(runner, component, feature_two);
    
    component::benchmark_component();
    
    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_Component() ? 0 : 1;
}
#endif
```

### 4.4 Key Testing Requirements

| Element | Requirement |
|---------|-------------|
| **Test runner** | Use `TestRunner` and `RUN_TEST_NS` macro |
| **Nested namespace** | Place test functions in `fat_p::testing::componentname` |
| **Function naming** | `bool test_xxx()` pattern via `TEST_CASE(xxx)` macro |
| **Assertions** | Use `FatPTest.h` macros: `SIMPLE_ASSERT`, `ASSERT_EQ`, `ASSERT_CLOSE`, etc. |
| **Benchmarks** | Use `measure_perf()`, `DoNotOptimize()`, `format_time()` |
| **No manual counting** | Never use manual `cout` with test counts |
| **No inline demos** | Tests and benchmarks only—no example/demo code |
| **Main function** | Always include `#ifdef ENABLE_TEST_APPLICATION` guarded `main()` |

### 4.5 Test Macros Reference

| Macro | Usage | Description |
|-------|-------|-------------|
| `TEST_CASE(name)` | `TEST_CASE(feature_one) { ... }` | Defines `bool test_feature_one()` |
| `RUN_TEST(runner, name)` | `RUN_TEST(runner, feature_one)` | Runs test from current namespace |
| `RUN_TEST_NS(runner, ns, name)` | `RUN_TEST_NS(runner, component, feature_one)` | Runs `ns::test_name` from nested namespace |

---

## 5. Documentation Standards

### 5.1 Required Manual Sections

| Section | Purpose |
|---------|---------|
| **What is [Component]?** | Problem statement with concrete code examples of the bad situation; survey of C++ landscape; where this component fits |
| **Core Architecture** | How it works internally (design, not just API); why decisions were made; performance characteristics explained |
| **Getting Started** | Prerequisites, integration, first complete program |
| **Feature Sections** | Comprehensive API coverage with examples; edge cases and gotchas |
| **Performance** | Benchmark methodology, test environment, results tables, interpretation |
| **Comparison** | Side-by-side tables with alternatives; code examples; clear verdicts |
| **Migration Guide** | Step-by-step from existing approaches; incremental adoption strategy |
| **Best Practices** | When to use (and not); naming conventions; API patterns |
| **Troubleshooting** | Common issues with symptoms/solutions; compilation errors; runtime errors |
| **Summary** | Key features; performance profile; quick start code; related components |

### 5.2 Documentation Philosophy

**Manuals are teaching documents, not API summaries.**

Assume the reader is intelligent but not necessarily an expert C++ programmer, and likely unfamiliar with this specific problem domain. They need to understand *what* each feature is, *why* it exists, and *when* to use it—not just *how* to call the API. Explain concepts that experienced developers might take for granted (atomics, lock-free algorithms, ADL) without being condescending.

**Every major section should answer:**

1. **What is this?** — Define the concept in plain terms
2. **Why does it exist?** — What problem does it solve? What happens without it?
3. **When should I use it?** — Decision criteria, trade-offs, alternatives
4. **How do I use it?** — API with complete, compilable examples

**Anti-pattern to avoid:**

```markdown
## Sinks

### ConsoleSink
Writes to console.

### FileSink  
Writes to file.

### AsyncSink
Asynchronous logging.
```

This tells the reader nothing useful. They can see the class names. What they need is:

```markdown
## Sinks

### What is a Sink?
A sink is a destination for log output. The name comes from the dataflow 
metaphor: logs flow from source (your code) to sink (output). DiagnosticLogger 
separates *what* you log from *where* it goes, enabling multiple outputs, 
different formats per destination, and hot-swapping without code changes.

### ConsoleSink
**What:** Writes to stdout and stderr based on log level.
**Why:** The most common output during development. Routing errors to stderr 
allows shell redirection (`./app 2>errors.log`).
**When:** Development, debugging, simple deployments.
```

### 5.3 Documentation Style

- **Code before explanation** — Show, then tell
- **Explains "why" before "how"** — Motivation before mechanics
- **Tables for comparisons** — Easier to scan than prose
- **Teaching moments** — Explain the "why" without being condescending
- **No jargon without purpose** — Accessible without dumbing down
- **Explicit namespace qualification** — Always use `fat_p::`, etc. (or local `using` directives)
- **Complete, compilable examples** — Not fragments

### 5.4 Comparison Sections

When comparing to external libraries, **assume the reader doesn't know them**. Don't just list feature differences—provide context:

**Bad:**
```markdown
| Feature | Ours | spdlog | glog |
|---------|------|--------|------|
| Header-only | Yes | Optional | No |
```

**Good:**
```markdown
### The C++ Logging Ecosystem

Before comparing features, it helps to understand where each library comes from:

**spdlog** is the de facto standard for modern C++ logging. Created by Gabi 
Melman in 2014, it emphasizes speed and ease of use. Used by Microsoft, Intel, 
and countless open-source projects. If you're starting fresh without strict 
dependency requirements, spdlog is often the default recommendation.

**glog** (Google Logging) was open-sourced by Google in 2008. It introduced 
conventions like `LOG(INFO)` syntax that many C++ developers recognize. 
Battle-tested at massive scale within Google.

[Then show the feature comparison table with context established]
```

This approach:
- Respects the reader's intelligence while filling knowledge gaps
- Provides decision-making context, not just facts
- Helps readers understand *why* they might choose one over another

### 5.5 Diagram Guidelines

Use Mermaid diagrams for complex concepts:

| Diagram Type | Best For |
|--------------|----------|
| `flowchart` | Control flow, decision trees, validation pipelines |
| `sequenceDiagram` | API call sequences, message passing |
| `classDiagram` | Class relationships, memory layouts |
| `stateDiagram-v2` | State machines, lifecycle transitions |
| `graph` | Problem/solution contrasts, data flow |

**When to add diagrams:**
- Multi-step processing pipelines
- Lock-free algorithms (CAS loops, retry patterns)
- Policy interactions
- Migration phases
- Trust boundaries
- Architecture relationships

**Mermaid syntax restrictions:**
- Avoid `()` in node labels (parsed as shape)
- Avoid `>=`, `<=`, `<>` (parsed as delimiters)
- Avoid `{}` (parsed as diamond shape)
- Avoid HTML tags
- Use `~~~` between subgraphs for vertical stacking
- Keep labels concise

---

## 6. Benchmark Environment Reference

### 6.1 Windows Test Machine

**Hardware:**

| Component | Specification |
|-----------|---------------|
| Processor | Intel Core i7-8850H @ 2.60 GHz |
| RAM | 32.0 GB |
| Architecture | x64 |

**Debug build (MSVC 2022):**
```
/std:c++17 /Od /ZI /RTC1 /MDd /EHsc /W3 /sdl /GS
/D "_DEBUG" /D "_CONSOLE" /D "NOMINMAX" /D "WIN32_LEAN_AND_MEAN"
```

**Release build (MSVC 2022 — for benchmarks):**
```
/std:c++17 /O2 /DNDEBUG /MD /EHsc /W3
/D "NOMINMAX" /D "WIN32_LEAN_AND_MEAN"
```

> **Critical:** Benchmarks must always run with Release/optimized builds (`/O2` or `-O3`), never Debug builds which disable optimizations and add runtime checks.

---

## 7. Quick Reference Checklist

### Before Submitting Code:

- [ ] Compiles successfully
- [ ] No truncated code
- [ ] No AI process comments (`NEW`, `FIXED`, etc.)
- [ ] Lines wrapped at 100 columns
- [ ] No special symbols or unusual characters
- [ ] File names unchanged
- [ ] Class names unchanged
- [ ] Download link provided (if files modified)

### Before Submitting Tests:

- [ ] Uses `TestRunner` + `RUN_TEST_NS` macro
- [ ] Test functions in nested namespace (`fat_p::testing::componentname`)
- [ ] Functions named `bool test_xxx()` via `TEST_CASE(xxx)`
- [ ] Uses `FatPTest.h` assertions
- [ ] Benchmarks use `DoNotOptimize`
- [ ] Clean header with just declaration
- [ ] No inline demos
- [ ] Includes `#ifdef ENABLE_TEST_APPLICATION` guarded `main()`

### Before Submitting Documentation:

- [ ] All required sections present
- [ ] Each section explains **what**, **why**, and **when**—not just how
- [ ] No "API summary" sections—every feature has context and rationale
- [ ] Comparison sections introduce external libraries before comparing
- [ ] Complete, compilable examples
- [ ] Tables for comparisons
- [ ] Explicit namespace qualification (or local `using` directives)
- [ ] No `using namespace` at global scope (local scope OK)
- [ ] Diagrams where appropriate
