# Fat-P Library Development Guidelines

## Document Governance

This is the **authoritative** Fat-P guideline document. Six documents form the complete governance set:

| Document | Role | Authority |
|----------|------|-----------|
| **Development Guidelines** (this) | Normative rules, AI behavior, code standards | HIGHEST -- this document wins |
| **Teaching Documents Style Guide** | All teaching docs: Overviews, User Manuals, Companion Guides, Case Studies, Foundations, Handbooks, Pattern Guides, Design Notes, Benchmark Results | PRIMARY for all documentation |
| **Test Suite Style Guide** | Test structure, coverage, assertions | PRIMARY for test code |
| **Benchmark Code Style Guide** | Benchmark methodology, statistics, competitor comparison | PRIMARY for benchmark code |
| **Systemic Hygiene Policy** | Header composability, ODR safety, namespace collision prevention | NORMATIVE for header correctness |
| **Human Guidance** | Instructions for human maintainers | REFERENCE (includes AI Capability Estimates) |

**Precedence rules:**
- Development Guidelines override all other documents
- Overlap between documents is intentional
- Each document must be standalone
- No document assumes another has been read

**Which document do I write?**

| Question | Document |
|----------|----------|
| "Should I use this component?" | Overview (see Teaching Documents Style Guide) |
| "How do I use this component?" | User Manual (see Teaching Documents Style Guide) |
| "Why is it designed this way?" | Companion Guide (see Teaching Documents Style Guide) |
| "Why did this fail, and how do I fix it?" | Case Study (see Teaching Documents Style Guide) |
| "What background do I need?" | Foundations (see Teaching Documents Style Guide) |
| "What discipline should teams adopt?" | Handbook (see Teaching Documents Style Guide) |
| "How do I apply this pattern?" | Pattern Guide (see Teaching Documents Style Guide) |
| "What decision did we make?" | Design Note (see Teaching Documents Style Guide) |
| "How does this perform?" | Benchmark Results (see Teaching Documents Style Guide) |
| "How do I test this component?" | Test Suite Style Guide |
| "How do I write a benchmark?" | Benchmark Code Style Guide |
| "Can these headers be included together?" | Systemic Hygiene Policy |
| "Is this code/test/doc compliant?" | Development Guidelines |

---

## 1. Library Design Principles

### 1.1 Core Technical Requirements

| Requirement | Specification |
|-------------|---------------|
| **C++ Standard** | C++20 default; C++17 minimum (see §1.1.1) |
| **Architecture** | Header-only |
| **Dependencies** | std + permitted system APIs/intrinsics; no third-party libraries (see §1.6) |
| **Weight** | Lightweight |
| **Target Domain** | HPC (High-Performance Computing) and Scientific Computing |

#### 1.1.1 C++ Standard Policy

**Default build standard:** C++20

**Minimum guaranteed support:** C++17 for the following layers only:
- `@layer Foundation`
- `@layer Containers`
- `@layer Concurrency`

**Best-effort C++17 support:** `Domain`, `Integration`, `Testing`
- These layers may freely use C++20 features when they materially improve correctness, clarity, or performance
- Preserve C++17 compatibility only when it can be done without spreading conditional compilation

#### 1.1.2 Centralized Feature Detection

All C++ standard and library feature detection must live in a single header (`CppStandardDetection.h`).

**Rules:**
- Other headers may **not** probe `__cplusplus`, `_MSVC_LANG`, or use feature-test macros directly
- Feature macros must represent actual library availability, not merely language mode
- Use standard feature-test macros (e.g., `__cpp_concepts`, `__cpp_lib_ranges`) where available

**Example `CppStandardDetection.h` pattern:**

```cpp
#pragma once

// Language standard detection
#if __cplusplus >= 202002L || (defined(_MSVC_LANG) && _MSVC_LANG >= 202002L)
    #define FATP_CPP20_OR_LATER 1
#else
    #define FATP_CPP20_OR_LATER 0
#endif

// Feature detection via standard feature-test macros
#if defined(__cpp_concepts) && __cpp_concepts >= 201907L
    #define FATP_HAS_CONCEPTS 1
#else
    #define FATP_HAS_CONCEPTS 0
#endif

#if defined(__cpp_lib_ranges) && __cpp_lib_ranges >= 201911L
    #define FATP_HAS_RANGES 1
#else
    #define FATP_HAS_RANGES 0
#endif

#if defined(__cpp_lib_source_location)
    #define FATP_HAS_SOURCE_LOCATION 1
#else
    #define FATP_HAS_SOURCE_LOCATION 0
#endif

#if defined(__cpp_lib_span) && __cpp_lib_span >= 202002L
    #define FATP_HAS_STD_SPAN 1
#else
    #define FATP_HAS_STD_SPAN 0
#endif

#if defined(__cpp_lib_three_way_comparison)
    #define FATP_HAS_SPACESHIP 1
#else
    #define FATP_HAS_SPACESHIP 0
#endif
```

#### 1.1.3 C++20 Feature Usage Rules

**In Foundation/Containers/Concurrency layers:**
- C++20 features are permitted only if:
  1. The feature is compile-time gated via `CppStandardDetection.h`
  2. A C++17 equivalent exists in the same component
  3. Both variants preserve semantics (diagnostics may differ)

**In higher layers (Domain/Integration/Testing):**
- Use C++20 freely without fallback
- Document if a component requires C++20 in the `@file` header

#### 1.1.4 Anti-Spaghetti Rules

Conditional code for standard differences lives only in:
- The centralized detection header (`CppStandardDetection.h`), or
- Narrow compatibility points such as `enforce` source-location capture

**Prohibited:**
- No "syntax emulation" macros (`FATP_CONCEPT`, `FATP_REQUIRES`, etc.)
- No `#if FATP_CPP20` scattered throughout headers

**Preferred:** One shared implementation with thin C++17 and C++20 front-ends where needed.

### 1.2 Design Philosophy

The library should fulfill "wish-lists" of programmers -- components should provide substantially more value than simple polyfills. Each component should solve real problems with thoughtful API design, comprehensive edge-case handling, and performance characteristics appropriate for HPC/scientific workloads.

### 1.3 Versioning & Compatibility

- No license requirements
- No version number above 1
- Library has never been released -- no need for deprecated features or backwards compatibility concerns
- **Never add backwards compatibility aliases** -- if something is renamed, it is renamed completely
- **Never design for "incremental adoption" or "gradual migration"** -- changes are atomic and complete
- **Never preserve broken patterns** -- if a design is wrong, fix it everywhere immediately
- **"Backward compatible" is not a virtue** -- do not weigh it as a positive in any design decision

**Rationale:** Backward compatibility concerns are the leading cause of API cruft, half-measures, and "legacy modes" that complicate codebases. Fat-P is pre-release; there are no external users to break. Every change should be the *correct* change, not the *safe* change.

### 1.4 Policy-Based Design

Policy template parameters are **optional**, not mandatory. Use them when:
- Users need custom hash functions, allocators, or comparators
- Behavior customization has concrete use cases

Do **NOT** use policies:
- "For future flexibility" without identified use cases
- When a simple default suffices for all known scenarios

Components should start simple and add policy parameters only when real needs emerge.

### 1.5 Separation of Concerns

Components should have focused responsibilities. When orthogonal concerns arise, prefer separate components over combined ones. For example:
- Basic containers vs. concurrent variants (`Queue` vs. `ConcurrentQueue`)
- Core functionality vs. diagnostic wrappers
- Data structures vs. serialization adapters

This principle requires judgment. The goal is zero-overhead for unused capabilities and clear expectations for users. When in doubt, prefer separation -- it's easier to compose simple components than to disable features of complex ones.

### 1.6 Dependency Policy

**Core rule:** No third-party libraries. Never include or depend on Boost, Abseil, fmt, Eigen, or similar.

**Allowed:**
- Standard library (`std`)
- System APIs and compiler intrinsics (POSIX, Windows, NUMA, SIMD) with proper gating and fallbacks

**Optional integration headers** (e.g., for MKL, OpenMP, CUDA, TensorFlow C API):
- May exist only in the `Integration` layer
- Must compile out cleanly when the external library is absent
- Must never be transitively included by lower layers
- Detection via `__has_include` is required

**Rationale:** Third-party dependencies create version conflicts, build complexity, and maintenance burden. Fat-P must remain buildable with only a standard-compliant compiler.

---

## 2. Layer System

### 2.1 Official Layers

Fat-P uses a six-layer architecture. Each header must declare exactly one layer via `@layer` tag:

```
Foundation → Containers → Concurrency → Domain → Integration → Testing
```

| Layer | Description | May Depend On |
|-------|-------------|---------------|
| **Foundation** | Core utilities, error handling, type traits | `std` + Foundation (same layer) |
| **Containers** | Data structures | Foundation |
| **Concurrency** | Threading primitives, lock-free structures | Foundation, Containers |
| **Domain** | Numerics, patterns, serialization, diagnostics | All below |
| **Integration** | External library bridges (MKL, CUDA, etc.) | All below |
| **Testing** | Test framework, benchmarks | All below |

**Layer dependency rule:** Components may only `#include` headers from layers **at or below** their own layer.

### 2.2 Layer Classification Requirements

Every header file must declare its architectural layer in the file-level Doxygen comment:

```cpp
/**
 * @file ComponentName.h
 * @brief One-line summary.
 *
 * @layer Containers
 *
 * [rest of description]
 */
```

**Rules:**
1. Components may only `#include` headers from layers **at or below** their own layer
2. Mismatch between `@layer` tag and actual includes is a **Critical** violation
3. AI and human reviewers must verify the `@layer` tag matches actual dependencies
4. Layer verification scripts treat any tag not in the canonical set (or the explicitly permitted legacy-mapped set) as an error

### 2.3 Domain Layer Clarification

Domain holds **first-class Fat-P components** that implement coherent abstractions: numerics (Tensor, CSRMatrix), diagnostics, HPC memory patterns, service-locator, state-machine, serialization, etc.

**Domain vs Integration:**
- Domain implements an abstraction using only Fat-P and std
- Integration bridges Fat-P abstractions to external systems (MKL, CUDA, OpenMP, TensorFlow)

**Allowed dependencies:** All lower layers.
**Forbidden:** Including Integration or Testing headers.

### 2.4 C++17 Guarantee by Layer

| Layer | C++17 Guarantee |
|-------|-----------------|
| Foundation | **Guaranteed** — must compile under C++17 |
| Containers | **Guaranteed** — must compile under C++17 |
| Concurrency | **Guaranteed** — must compile under C++17 |
| Domain | Best-effort — may use C++20 features |
| Integration | Best-effort — may use C++20 features |
| Testing | Best-effort — may use C++20 features |

### 2.5 Legacy Layer Mapping

For backward compatibility with existing `@layer` tags, the following mapping applies:

| Legacy Layer | Maps To | Notes |
|--------------|---------|-------|
| Infrastructure | Containers | Core data structures |
| CoreUtility | Foundation | Lightweight utilities |
| Enforcement | Foundation | Contracts, error handling |
| Policy | Domain | Policy-based components |
| Application | Domain | Full-featured components |
| Serialization | Domain | Format-specific I/O |

New headers should use the six canonical layer names. Existing headers may retain legacy names until migrated.

---

[... Sections 3-6 remain as in original document ...]

---

## Changelog

### v3.0 (January 2026)
- **BREAKING:** Updated C++ standard policy to C++20 default with bounded C++17 support
- Added Section 1.1.1-1.1.4: Detailed C++ standard policy with feature detection requirements
- Added Section 1.6: Explicit dependency policy (no third-party libraries)
- Reorganized Section 2: New six-layer architecture (Foundation/Containers/Concurrency/Domain/Integration/Testing)
- Added Section 2.4: C++17 guarantee by layer
- Added Section 2.5: Legacy layer mapping for migration
- Added `CppStandardDetection.h` requirement for centralized feature detection
- CI updates: Added C++17 compile gate, layer verifier, forbidden-deps scan

### v2.8 (January 2026)
- Expanded Section 4.2 Formatting Standards with complete clang-format configuration
- Updated line width policy: 100 columns typical, 120 columns absolute maximum
- Added PenaltyExcessCharacter mechanism to discourage lines over 100 columns
- Added comprehensive clang-format options for all formatting aspects
- Added key formatting rules summary table

[... rest of changelog as in original ...]

---

*Fat-P Library Development Guidelines v3.0 -- January 2026*
