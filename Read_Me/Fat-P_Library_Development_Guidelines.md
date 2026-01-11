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
| **C++ Standard** | C++17 minimum |
| **Architecture** | Header-only |
| **Dependencies** | Standard library only (no external dependencies beyond `std` and internal components) |
| **Weight** | Lightweight |
| **Target Domain** | HPC (High-Performance Computing) and Scientific Computing |

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

---

## 2. Naming Conventions

### 2.1 Core Naming Rules

**Rule 1 -- Names describe the invariant, not the algorithm**

Users should understand *what stays true*, not *how you implement it*.

| Good | Bad |
|------|-----|
| `FastHashMap` | `SwissTable` |
| `StableHashMap` | `RobinHoodHashMap` |
| `FlatMap` | `SortedVectorMap` |
| `SortedContainer` | `MultiPolicySortedVector` |

**Rule 2 -- Adjectives are semantic, not marketing**

Each adjective has a documented meaning:

| Adjective | Meaning |
|-----------|---------|
| `Aligned` | Memory alignment guarantees (cache line, SIMD) |
| `Atomic` | Lock-free atomic operations |
| `Binary` | Raw byte-level representation |
| `Checked` | Runtime validation enabled |
| `Circular` | Ring buffer semantics |
| `Concurrent` | Thread-safe operations |
| `Diagnostic` | Observability and debugging support |
| `Expected` | Result-or-error return type |
| `Fast` | Optimized for throughput; weaker guarantees |
| `Flat` | Contiguous storage |
| `LockFree` | Wait-free or lock-free progress guarantee |
| `Policy` | Behavior defined by template policies |
| `Small` | Inline storage optimization |
| `Sorted` | Order invariant maintained |
| `Sparse` | Efficient for sparse data |
| `Stable` | Stronger behavioral guarantees (e.g., no tombstones, predictable iteration) |
| `Strong` | Type-safe wrapper with distinct identity |

No vague adjectives. If an adjective isn't in this table, don't use it without adding it.

**Note:** These adjectives are permitted in component *names*. The vocabulary ban in Section 7.2 applies to *documentation prose*, not naming. For example, `FastHashMap` is a valid component name, but documentation should not describe it as "fast" -- instead say "O(1) average lookup" or "SIMD-accelerated".

**Rule 3 -- Sets and maps differ only by value semantics**

- `Map` -> key/value
- `Set` -> value-only

No `HashSetMap` or similar nonsense.

**Disallowed Naming Sources:**

| Category | Examples | Why Banned |
|----------|----------|------------|
| Algorithm names | Swiss, Robin Hood, Hopscotch, Cuckoo | Implementation detail, not invariant |
| Implementation details | Bucket, Probe, Chain, Slot | Exposes internals |
| Marketing adjectives | Ultra, Turbo, Super, Mega | Meaningless |
| Abbreviations | HM, SM, FC | Unreadable |

### 2.2 Canonical Container Names

**Hash-Based Containers:**
```cpp
FastHashMap<K, V>      // SIMD-accelerated, tombstone-based, maximum throughput
StableHashMap<K, V>    // Robin Hood, tombstone-free, predictable behavior
```

**Ordered Flat Containers:**
```cpp
FlatMap<K, V>          // Contiguous storage, sorted by key
FlatSet<T>             // Contiguous storage, sorted values
```

**Policy-Based Containers:**
```cpp
SortedContainer<T, Policies...>  // Sorted with configurable uniqueness/concurrency
```

### 2.3 Standardized Vocabulary

Use these terms **consistently** across all documentation:

| Term | Meaning |
|------|---------|
| Invariant | Property always maintained by the container |
| Contract | User responsibility (caller must ensure) |
| UB | Undefined behavior if contract violated |
| QoI | Quality-of-implementation behavior (not guaranteed) |
| Stable | Preserves documented semantics under mutation |
| Frozen | Immutable mode -- mutations cause UB in release, assertion in debug |
| Flat | Contiguous storage (single allocation) |
| Policy | Compile-time behavior selection via template parameter |

**Frozen semantics clarification:**
- `freeze()` enables higher load factors for read-only access
- `unfreeze()` returns to normal mutable mode
- Freezing is reversible and does not change the type
- Mutating a frozen container is UB in release builds

---

## 3. Code Review Protocol

### 3.1 Review Process

When asked to review code, perform a **deep analysis** covering:

1. **Errors** -- Bugs, undefined behavior, logic errors, memory issues
2. **Improvements** -- API design, performance, safety, maintainability
3. **Internal leverage** -- Identify where other library components could/should be used

### 3.2 Review Output Format

Provide analysis as a prioritized list with:
- Clear description of issue/improvement
- Code examples demonstrating the problem or solution
- Priority level (Critical / High / Medium / Low)
- Comments explaining rationale

### 3.3 Dependency Analysis

- If internal dependencies are not explicitly provided, assume they exist and compile
- If a thorough analysis requires seeing a dependency, **ask for it** before proceeding
- Do **not** generate code unless explicitly asked

---

## 4. Code Generation Rules

### 4.1 General Principles

| Rule | Detail |
|------|--------|
| **No code unless requested** | Do not generate code unless explicitly asked |
| **No explanatory files** | Do not generate `.md`, `.txt`, or explanation files unless requested |
| **Preserve naming** | Never change file names or internal class names when modifying components |
| **Complete code only** | **NEVER provide truncated code** -- always provide the entire file |
| **No AI comments** | Never include comments like `NEW`, `FIXED`, `BUGFIX`, `CHANGED` -- comments describe *what* the code does, not the editing process |
| **Always compile** | Compile code before delivering it |
| **Provide download links** | If files are modified, always provide download links **for modified files only** (do not attach unchanged files unless explicitly requested) |
| **No backwards compat** | Never add deprecated aliases or compatibility shims |

### 4.2 Formatting Standards

**Line width:**
- **Target:** 100 columns (typical)
- **Hard limit:** 120 columns (absolute maximum)

The `ColumnLimit` is set to 120 with a high `PenaltyExcessCharacter` to discourage lines over 100 columns while still allowing up to 120 when necessary.

**Style configuration (clang-format):**

```yaml
# Fat-P Library .clang-format
# Column policy: Target 100, Hard limit 120

Language: Cpp
Standard: c++17

BasedOnStyle: LLVM

# Indentation
UseTab: Never
IndentWidth: 4
TabWidth: 4
ContinuationIndentWidth: 4
ConstructorInitializerIndentWidth: 4
AccessModifierOffset: -4
IndentCaseLabels: true
IndentPPDirectives: None
NamespaceIndentation: None

# Braces - Allman style (opening brace on its own line)
BreakBeforeBraces: Allman

# Always use braces for control statements
InsertBraces: true

# Line length
ColumnLimit: 120

# Penalties to prefer breaking at 100 columns
PenaltyExcessCharacter: 1000000
PenaltyBreakBeforeFirstCallParameter: 19
PenaltyBreakComment: 300
PenaltyBreakFirstLessLess: 120
PenaltyBreakString: 1000
PenaltyBreakTemplateDeclaration: 10
PenaltyReturnTypeOnItsOwnLine: 60

# No short forms - always use full brace blocks
AllowShortBlocksOnASingleLine: Never
AllowShortCaseLabelsOnASingleLine: false
AllowShortEnumsOnASingleLine: false
AllowShortFunctionsOnASingleLine: None
AllowShortIfStatementsOnASingleLine: Never
AllowShortLambdasOnASingleLine: Empty
AllowShortLoopsOnASingleLine: false

# Parameter/argument packing - one per line when wrapping
AllowAllArgumentsOnNextLine: false
AllowAllParametersOfDeclarationOnNextLine: false
BinPackArguments: false
BinPackParameters: false

# Constructor initializers
BreakConstructorInitializers: BeforeComma
PackConstructorInitializers: Never

# Function declarations/definitions
AlwaysBreakAfterReturnType: None
AlwaysBreakTemplateDeclarations: Yes

# Alignment
AlignAfterOpenBracket: Align
AlignArrayOfStructures: None
AlignConsecutiveAssignments: None
AlignConsecutiveBitFields: None
AlignConsecutiveDeclarations: None
AlignConsecutiveMacros: None
AlignEscapedNewlines: Left
AlignOperands: Align
AlignTrailingComments: true

# Spacing
SpaceAfterCStyleCast: false
SpaceAfterLogicalNot: false
SpaceAfterTemplateKeyword: true
SpaceAroundPointerQualifiers: Default
SpaceBeforeAssignmentOperators: true
SpaceBeforeCaseColon: false
SpaceBeforeCpp11BracedList: false
SpaceBeforeCtorInitializerColon: true
SpaceBeforeInheritanceColon: true
SpaceBeforeParens: ControlStatements
SpaceBeforeRangeBasedForLoopColon: true
SpaceBeforeSquareBrackets: false
SpaceInEmptyBlock: false
SpaceInEmptyParentheses: false
SpacesBeforeTrailingComments: 1
SpacesInAngles: Never
SpacesInCStyleCastParentheses: false
SpacesInConditionalStatement: false
SpacesInContainerLiterals: true
SpacesInLineCommentPrefix:
  Minimum: 1
  Maximum: 1
SpacesInParentheses: false
SpacesInSquareBrackets: false

# Pointer/reference alignment
DerivePointerAlignment: false
PointerAlignment: Left
ReferenceAlignment: Left

# Includes
SortIncludes: CaseInsensitive
IncludeBlocks: Preserve

# Namespace comments
FixNamespaceComments: true
ShortNamespaceLines: 0

# Empty lines
EmptyLineAfterAccessModifier: Never
EmptyLineBeforeAccessModifier: LogicalBlock
KeepEmptyLinesAtTheStartOfBlocks: false
MaxEmptyLinesToKeep: 2
SeparateDefinitionBlocks: Leave

# Misc
BreakBeforeBinaryOperators: None
BreakBeforeConceptDeclarations: true
BreakBeforeTernaryOperators: true
BreakStringLiterals: true
CompactNamespaces: false
Cpp11BracedListStyle: true
IndentExternBlock: NoIndent
IndentGotoLabels: false
IndentWrappedFunctionNames: false
InsertTrailingCommas: None
LambdaBodyIndentation: Signature
ReflowComments: true
SortUsingDeclarations: true
```

**Key formatting rules summary:**

| Setting | Value | Effect |
|---------|-------|--------|
| `BreakBeforeBraces` | Allman | Opening braces on their own line |
| `IndentWidth` | 4 | 4-space indentation |
| `UseTab` | Never | Spaces only, no tabs |
| `ColumnLimit` | 120 | Hard limit (target 100 via penalties) |
| `InsertBraces` | true | Always use braces for `if`/`for`/`while` |
| `BinPackParameters` | false | One parameter per line when wrapping |
| `NamespaceIndentation` | None | No extra indent inside namespaces |
| `PointerAlignment` | Left | `int* ptr` not `int *ptr` |

### 4.3 Naming Conventions

| Element | Style | Example |
|---------|-------|---------|
| Class/Struct | PascalCase | `StableHashMap`, `CircularBuffer` |
| Function/Method | camelCase | `findEntry()`, `insertOrAssign()` |
| Instance member variable | `m` prefix + PascalCase | `mBucketCount`, `mLoadFactor` |
| Static member variable | `s` prefix + PascalCase | `sInstanceCount`, `sDefaultPolicy` |
| Local variable | camelCase | `entryIndex`, `hashValue` |
| Template parameter | PascalCase | `Key`, `Value`, `Policy` |
| Preprocessor constant/macro | SCREAMING_SNAKE | `MAX_LOAD_FACTOR`, `BUFFER_SIZE` |
| Compile-time constant (`constexpr`) | `k` prefix + PascalCase | `kDefaultCapacity`, `kMaxRetries` |
| Namespace | lowercase | `fat_p`, `fat_p::detail` |

**Member Variable Rationale:**

The `m` prefix with PascalCase (e.g., `mBucketCount`) is mandatory for **instance** member variables.

**Why this convention:**
- **Disambiguation without `this->`**: In header-only code, parameter names often shadow member names. The `m` prefix eliminates ambiguity without requiring `this->` throughout the codebase.
- **AI code generation**: Explicit member identification helps code generation tools produce correct code without context about class structure.
- **Grep-ability**: `mFoo` is trivially searchable as a member; `foo` could be anything.

While modern style guides vary on member prefixes, this convention is load-bearing for header-only libraries where disambiguation matters.

**Static Member Clarification:**

The `m` prefix applies to **instance members** only. Static members use `s` prefix with PascalCase (e.g., `sInstanceCount`). This distinction matters because:
- Static members are accessed via `ClassName::sVariable`, making the prefix redundant for disambiguation but useful for grep-ability
- Instance vs. static is a semantic difference worth encoding in the name
- `inline static` members in headers are particularly common in Fat-P; the `s` prefix immediately signals shared-across-instances semantics

```cpp
class ConnectionPool
{
private:
    size_t mPoolSize;                                    // Instance member
    inline static std::atomic<size_t> sInstanceCount{0}; // Static member
    static constexpr size_t kMaxConnections = 100;       // Static constant (see below)
};
```

**Constants Convention Rationale:**

The dual convention distinguishes preprocessor definitions from type-safe constants:
- `SCREAMING_SNAKE` signals "this is a macro" -- preprocessor text substitution with no type safety
- `kPrefix` signals "this is a `constexpr` value" -- type-safe, scoped, debugger-visible

This distinction helps readers immediately understand whether they're dealing with textual substitution or a proper C++ constant:

```cpp
// Preprocessor -- SCREAMING_SNAKE
#define FATP_CACHE_LINE_SIZE 64       // Macro: no type, no scope

// Compile-time constants -- kPrefix
static constexpr size_t kCacheLineSize = 64;           // Typed, scoped
static constexpr float kDefaultLoadFactor = 0.875f;    // Typed, scoped
inline constexpr std::string_view kVersionString = "1.0.0";
```

**Exception:** Template parameters that represent compile-time values (e.g., `template <size_t N>`) do not use `k` prefix -- they follow the PascalCase template parameter convention.

**Function names are descriptive:**

```cpp
// Good - describes what it does
void insertWithDisplacement(size_t slot, Entry&& entry);
bool tryGrowAndRehash();
size_t computeProbeDistance(size_t slot, size_t idealSlot);

// Bad - vague or abbreviated
void insert2(size_t s, Entry&& e);
bool grow();
size_t dist(size_t a, size_t b);
```

**Member variable examples:**

```cpp
class StableHashMap
{
private:
    Entry* mEntries;           // Not: entries_, m_entries, entries
    size_t mBucketCount;       // Not: bucket_count, _bucketCount
    float mMaxLoadFactor;      // Not: max_load_factor
    bool mFrozen;              // Not: is_frozen, frozen_
};
```

### 4.4 Header Guards

Use `#pragma once` exclusively. Do not use `#ifndef`/`#define`/`#endif` include guards.

```cpp
// Correct
#pragma once

namespace fat_p {
// ...
}

// Wrong
#ifndef COMPONENT_H
#define COMPONENT_H
// ...
#endif
```

### 4.5 Header-Only Enforcement

Fat-P is header-only. All code lives in `.h` files. This requires understanding C++ linkage rules.

**What requires `inline` keyword:**

| Entity | Needs `inline`? | Why |
|--------|-----------------|-----|
| Function defined in header (namespace scope) | **YES** | Prevents ODR violation |
| Variable defined in header (namespace scope) | **YES** (`inline` or `constexpr`) | Prevents ODR violation |
| Function defined inside class body | No | Implicitly inline |
| Static member variable | **YES** (`inline static`) | Prevents ODR violation |
| Template function/class | No | Templates have special linkage |
| `constexpr` function | No | Implicitly inline |
| `constexpr` variable | No | Implicitly inline |

**Anonymous namespaces in headers:**

Anonymous namespaces in headers are **strongly discouraged**. They create per-TU copies of everything inside, causing:
- Silent state duplication (each TU gets its own copy)
- Binary bloat
- Subtle bugs when "shared" state isn't shared

**Acceptable use:** Stateless, compile-time-only constants where TU-local copies are harmless.

**Preferred alternative:** Use `namespace fat_p::detail` with `inline` for shared definitions.

```cpp
// STRONGLY DISCOURAGED - creates per-TU state
namespace {
    int counter = 0;  // Each TU gets its own counter!
}

// CORRECT - single definition across TUs
namespace fat_p::detail {
    inline int counter = 0;  // Shared across TUs
}

// ACCEPTABLE - stateless, compile-time only
// Anonymous namespace: compile-time constant, no mutable state
namespace {
    constexpr int kBufferSize = 1024;  // OK: compile-time constant, no ODR issue
}
```

**Anonymous Namespace Justification Requirements:**

When an anonymous namespace is used in a header, a justification comment is **required immediately above** the namespace declaration. The comment must explain why TU-local copies are acceptable.

**Format:**
```cpp
// Anonymous namespace: [justification]
namespace {
    // ...
}
```

**Acceptable justifications:**

| Justification | When Valid |
|---------------|------------|
| "compile-time constant, no mutable state" | `constexpr` values only |
| "stateless type traits" | Empty structs used for tag dispatch or SFINAE |
| "TU-local intentional for [reason]" | Rare cases where per-TU copies are the design |

**Examples:**

```cpp
// CORRECT -- justified compile-time constant
// Anonymous namespace: compile-time constant, no mutable state
namespace {
    constexpr size_t kBufferSize = 1024;
    constexpr std::array<int, 4> kMagicNumbers = {1, 2, 3, 4};
}

// CORRECT -- justified stateless trait
// Anonymous namespace: stateless type trait for SFINAE, no ODR concern
namespace {
    template <typename T, typename = void>
    struct HasValueType : std::false_type {};
    
    template <typename T>
    struct HasValueType<T, std::void_t<typename T::value_type>> : std::true_type {};
}

// WRONG -- no justification
namespace {
    constexpr int kTimeout = 30;  // Missing justification comment!
}

// WRONG -- mutable state (never acceptable in headers)
// Anonymous namespace: ??? (no valid justification exists)
namespace {
    int gCounter = 0;  // REJECTED: mutable state creates per-TU copies
}
```

**When no valid justification exists:** Use `namespace fat_p::detail` with `inline` instead. If you cannot write a justification comment, that's a signal that anonymous namespace is the wrong choice.

```cpp
// PREFERRED alternative to anonymous namespace
namespace fat_p::detail {
    inline int sharedCounter = 0;  // Single definition across all TUs
}
```

### 4.6 Documentation Comments

**Two types of comments serve different purposes:**

| Type | Purpose | Audience |
|------|---------|----------|
| **Doxygen** (`/** */`) | Contract specification | IDE tooltips, API reference |
| **Regular** (`//`) | Design rationale, implementation notes | Code readers, maintainers |

**Doxygen comments** should be:
- **Brief** -- one line for `@brief`, 2-3 lines total maximum
- **Contract-focused** -- parameters, return values, preconditions, exceptions
- **No duplication** -- design rationale belongs in manuals, not Doxygen

```cpp
/**
 * @brief Finds value by key
 * @param key The key to search for
 * @return Pointer to value, or nullptr if not found
 */
Value* find(const Key& key);
```

**Regular comments** should be extensive where needed:
- Explain *why* a design decision was made
- Document non-obvious algorithmic choices
- Note performance characteristics
- Warn about subtle gotchas

```cpp
// Robin Hood insertion: when inserting a new element, if we encounter an
// existing element with a shorter probe distance, we swap them and continue
// inserting the displaced element. This bounds the variance in probe distances
// and keeps worst-case lookup time predictable.
//
// The probe distance is stored implicitly via the difference between an
// element's actual position and its ideal position (hash & mask).
void insertImpl(size_t hash, Key&& key, Value&& value)
{
    // ... implementation
}
```

**Anti-pattern -- Doxygen with implementation details:**
```cpp
// Wrong: exposes algorithm in Doxygen
/**
 * @brief Finds value using Robin Hood probing with backward shift
 * @details Uses H2 hash comparison for SIMD acceleration, then falls back
 *          to full key comparison. Probe sequence uses triangular stepping.
 */
Value* find(const Key& key);
```

The test for whether something belongs in Doxygen: *"Would this make sense as an IDE tooltip?"* If the answer is no, use a regular comment instead.

### 4.7 Doxygen Standards

#### 4.7.1 When to Use Doxygen Comments

| Element | Doxygen Required? | Notes |
|---------|:-----------------:|-------|
| Public class/struct | Yes | Brief + detailed description |
| Public method | Yes | Brief + params + return + exceptions |
| Public type alias | Yes | Brief description |
| Public constant | Yes | Brief + value explanation |
| Private members | Optional | Only if non-obvious |
| Implementation details | No | Use regular `//` comments |

#### 4.7.2 Comment Syntax

**Use `///` for Single-Line Briefs:**

```cpp
/// Returns the number of elements in the container.
[[nodiscard]] size_t size() const noexcept;
```

**Use `/** */` for Multi-Line Documentation:**

```cpp
/**
 * @brief Inserts a key-value pair if the key does not exist.
 *
 * If the key already exists, the map is unchanged and false is returned.
 * This matches std::unordered_map::insert() semantics.
 *
 * @param key The key to insert.
 * @param value The value to associate with the key.
 * @return true if insertion occurred, false if key already existed.
 *
 * @note O(1) amortized. May trigger rehash if load factor exceeded.
 * @see insertOrAssign() for upsert behavior.
 */
bool insert(const Key& key, const Value& value);
```

#### 4.7.3 Required Tags

**For Classes/Structs:**

| Tag | Required | Purpose |
|-----|:--------:|---------|
| `@brief` | Yes | One-line summary |
| `@tparam` | Yes (if templated) | Template parameter descriptions |
| `@note` | Optional | Important usage notes |
| `@see` | Optional | Related classes/functions |

**Example:**

```cpp
/**
 * @brief A Robin Hood hash map with tombstone-free deletion.
 *
 * StableHashMap provides O(1) average-case insert, find, and erase
 * operations with backward-shift deletion that prevents probe distance
 * degradation under sustained churn.
 *
 * @tparam Key The key type. Must be DefaultConstructible.
 * @tparam Value The value type. Must be DefaultConstructible.
 * @tparam Policy Controls hash function, equality, and allocator.
 *
 * @note Not thread-safe. Use external synchronization for concurrent access.
 * @see FastHashMap for SIMD-accelerated alternative with different tradeoffs.
 */
template <typename Key, typename Value, typename Policy = DefaultPolicy<Key, Value>>
class StableHashMap;
```

**For Methods:**

| Tag | Required | Purpose |
|-----|:--------:|---------|
| `@brief` | Yes | One-line summary |
| `@param` | Yes (if params exist) | Parameter descriptions |
| `@return` | Yes (if non-void) | Return value description |
| `@throws` | Yes (if can throw) | Exception conditions |
| `@pre` | Optional | Preconditions (contracts) |
| `@post` | Optional | Postconditions |
| `@note` | Optional | Important caveats |
| `@warning` | Optional | Dangerous usage patterns |
| `@see` | Optional | Related functions |

#### 4.7.4 Complexity and Thread-Safety Tags

Fat-P requires explicit complexity and thread-safety documentation.

**Complexity -- use `@note` with standardized format:**

```cpp
/**
 * @brief Finds the value associated with a key.
 *
 * @param key The key to search for.
 * @return Pointer to value if found, nullptr otherwise.
 *
 * @note Complexity: O(1) average, O(n) worst-case.
 */
Value* find(const Key& key);
```

**Thread-Safety -- use `@note` with explicit guarantee level:**

```cpp
/**
 * @brief Removes all elements from the container.
 *
 * @note Thread-safety: NOT thread-safe. Caller must synchronize.
 */
void clear() noexcept;
```

**Standardized thread-safety phrases:**

| Phrase | Meaning |
|--------|---------|
| "NOT thread-safe" | No concurrent access allowed |
| "Thread-safe for concurrent reads" | Multiple readers OK, no writers |
| "Thread-safe" | Full concurrent access OK |
| "Lock-free" | Progress guarantee under contention |
| "Wait-free" | Bounded operations |

#### 4.7.5 Doxygen Style Guidelines

Doxygen comments should be technically precise. While the banned vocabulary list from Section 7.2 does **not** apply to Doxygen, prefer mechanism-specific language when it improves clarity:

| Less Precise | More Precise |
|--------------|--------------|
| "Fast lookup" | "O(1) average-case lookup" |
| "Safe deletion" | "Bounds-checked deletion" |
| "Efficient storage" | "Contiguous cache-friendly storage" |

This is a recommendation for clarity, not a requirement. The terms in the left column are acceptable in Doxygen comments.

#### 4.7.6 Formatting Standards

**Line Width:** Doxygen comments follow the same 100-column limit as code.

**Alignment:** Align `@param` and `@return` descriptions if it improves readability:

```cpp
/**
 * @param key   The key to insert.
 * @param value The value to associate.
 * @return      true if inserted, false if key existed.
 */
```

**Code Examples in Doxygen:** Use `@code` / `@endcode` for inline examples:

```cpp
/**
 * @brief Reserves space for at least n elements.
 *
 * @code
 * StableHashMap<int, std::string> map;
 * map.reserve(1000);  // Pre-allocate for 1000 elements
 * @endcode
 *
 * @param n Minimum number of elements to reserve space for.
 */
void reserve(size_t n);
```

#### 4.7.7 What NOT to Document with Doxygen

**Implementation Details:**

```cpp
// GOOD: Regular comment for implementation detail
// Uses backward-shift to maintain probe distance invariant
void eraseImpl(size_t slotIndex);

// BAD: Doxygen for private implementation
/** @brief Internal erase implementation. */  // Don't do this
void eraseImpl(size_t slotIndex);
```

**Obvious Accessors:**

```cpp
// Acceptable: Minimal Doxygen for trivial accessor
/// Returns true if the container is empty.
[[nodiscard]] bool empty() const noexcept;

// Overkill:
/**
 * @brief Checks whether the container is empty.
 * @return true if the container contains no elements, false otherwise.
 * @note Complexity: O(1).
 * @note Thread-safety: Thread-safe for concurrent reads.
 */  // Too much for empty()
```

#### 4.7.8 File Headers

Every header file must have a Doxygen file comment that includes the **layer classification**:

```cpp
/**
 * @file StableHashMap.h
 * @brief Robin Hood hash map with tombstone-free deletion.
 *
 * @layer Infrastructure
 *
 * This header provides StableHashMap, a hash table implementation
 * optimized for sustained insert/erase workloads where tombstone
 * accumulation would degrade performance.
 *
 * @see StableHashMap_User_Manual.md for usage documentation.
 * @see StableHashMap_Companion_Guide.md for design rationale.
 */

#pragma once
```

The `@layer` tag is **mandatory** for all headers. See Section 6.5 for layer definitions.

#### 4.7.9 Doxygen Configuration

**Recommended Doxyfile Settings:**

```
# Project
PROJECT_NAME           = "Fat-P Library"
PROJECT_BRIEF          = "Header-only C++17 utilities for HPC"

# Input
INPUT                  = .
FILE_PATTERNS          = *.h
RECURSIVE              = NO
EXCLUDE_PATTERNS       = test_* benchmark_*

# Extraction
EXTRACT_ALL            = NO
EXTRACT_PRIVATE        = NO
EXTRACT_STATIC         = NO
HIDE_UNDOC_MEMBERS     = YES
HIDE_UNDOC_CLASSES     = YES

# Output
GENERATE_HTML          = YES
GENERATE_LATEX         = NO
HTML_OUTPUT            = docs/api

# Warnings
WARN_IF_UNDOCUMENTED   = YES
WARN_NO_PARAMDOC       = YES
```

### 4.8 Prohibited Content

- No special symbols or unusual Unicode characters in code
- No `using namespace` at **global scope** in examples (local scope is fine for brevity)
- No fictional macros -- only document what exists in code

### 4.9 Using Directives Policy

| Scope | Allowed | Example |
|-------|---------|---------|
| Global scope | **NO** | `using namespace fat_p::diagnostic;` at file level |
| Function/block scope | **YES** | `void foo() { using namespace fat_p::diagnostic; ... }` |
| Namespace alias | **YES** | `namespace diag = fat_p::diagnostic;` |

Local `using` directives improve readability in examples without polluting the global namespace.

**Cross-reference:** For comprehensive header composability rules including namespace flattening prohibition and root namespace restrictions, see the *Systemic Hygiene Policy*.

---

## 5. Unit Testing Standards

### 5.1 Testing Philosophy

- **Thorough testing** with 100% coverage goal
- Consider all corner cases and edge conditions
- Tests should validate both happy paths and failure modes

### 5.2 Testing Pattern (Reference Files)

Study these files for the canonical pattern:
- `test_BitSet.h` -- Header pattern
- `test_BitSet.cpp` -- Implementation pattern  
- `FatPTest.h` -- Framework and assertions

### 5.3 Test File Structure

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

void benchmarkComponent()
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
    
    component::benchmarkComponent();
    
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

### 5.4 Key Testing Requirements

| Element | Requirement |
|---------|-------------|
| **Test runner** | Use `TestRunner` and `RUN_TEST_NS` macro |
| **Nested namespace** | Place test functions in `fat_p::testing::componentname` |
| **Function naming** | `bool test_xxx()` pattern via `TEST_CASE(xxx)` macro |
| **Assertions** | Use `FatPTest.h` macros: `SIMPLE_ASSERT`, `ASSERT_EQ`, `ASSERT_CLOSE`, etc. |
| **Benchmarks** | Use `measure_perf()`, `DoNotOptimize()`, `format_time()` |
| **No manual counting** | Never use manual `cout` with test counts |
| **No inline demos** | Tests and benchmarks only -- no example/demo code |
| **Main function** | Always include `#ifdef ENABLE_TEST_APPLICATION` guarded `main()` |

### 5.5 Test Macros Reference

| Macro | Usage | Description |
|-------|-------|-------------|
| `TEST_CASE(name)` | `TEST_CASE(feature_one) { ... }` | Defines `bool test_feature_one()` |
| `RUN_TEST(runner, name)` | `RUN_TEST(runner, feature_one)` | Runs test from current namespace |
| `RUN_TEST_NS(runner, ns, name)` | `RUN_TEST_NS(runner, component, feature_one)` | Runs `ns::test_name` from nested namespace |

---

## 6. Container Documentation Standards

### 6.1 Required Container Documentation Sections

Every container gets these 6 sections, in this order:

**1. Intent**
> What problem does this container solve?

**2. Invariant**
> What is always true about this container's state?

**3. Complexity Model**
> How expensive are operations?

| Operation | Complexity |
|-----------|------------|
| find | O(1) avg |
| insert | O(N) |
| erase | O(N) |

**4. Stability & Invalidations**
> When do iterators, references, and pointers break?

Use this table format for every container:

| Operation | Iterators | References | Pointers |
|-----------|-----------|------------|----------|
| insert | Invalidated | Invalidated | Invalidated |
| erase | Invalidated | Invalidated | Invalidated |
| find | Valid | Valid | Valid |
| rehash | Invalidated | Invalidated | Invalidated |

**5. Concurrency Model**
> What is thread-safe? What is not?

Even if the answer is "nothing is thread-safe", state it explicitly. Use standardized phrases:
- "Not thread-safe"
- "Thread-safe for concurrent reads only"
- "Fully thread-safe (all operations)"
- "Thread-safe with external synchronization"

**6. When to Use / When Not to Use**
> Decision criteria for choosing this container.

### 6.2 Container Selection Guide

Maintain one global table:

| Need | Container |
|------|-----------|
| Fast key lookup | `FastHashMap` |
| Predictable mutation semantics | `StableHashMap` |
| Small ordered map | `FlatMap` |
| Ordered set | `FlatSet` |
| Sorted + fuzzy + concurrent | `SortedContainer` |

No overlap, no ambiguity.

### 6.3 Error Reporting Convention

**Rule 4 -- Error reporting matches container weight**

| Container Type | Error Handling |
|----------------|----------------|
| Lightweight (`FastHashMap`, `FlatMap`) | UB on contract violation; assertions in debug |
| Policy containers (`SortedContainer`) | Policy determines error handling |
| Safe wrappers | Return `Expected<T>` or throw |

### 6.4 Architectural Layers and Allowed Dependencies

Components exist in layers. Lower layers cannot depend on higher layers.

| Layer | Examples | Allowed Dependencies |
|-------|----------|---------------------|
| Infrastructure | `FastHashMap`, `StableHashMap`, `SmallVector` | `std` only |
| CoreUtility | `Stringify`, `TypeTraits`, `DiagnosticLogger_Core`, `FloatingPointComparison` | `std` + Infrastructure |
| Enforcement | `enforce.h`, `Expected.h`, `ContractException` | `std` + Infrastructure + CoreUtility |
| Policy | `SortedContainer`, `ConcurrencyPolicies` | All below |
| Application | `EqualityComparisons`, `Tensor`, `DiagnosticLogger` (full) | All below |
| Serialization | `FatPJson`, `FatPCbor`, `BinaryLite` | All below |

**Critical distinction:**

- `DiagnosticLogger_Core.h` = **CoreUtility** (minimal logging hook, usable everywhere)
- `DiagnosticLogger.h` (full system) = **Application** (rich features, higher dependencies)

Headers with a `_Core` suffix are lightweight base components intended for broad use. The full-featured version (without suffix) lives in a higher layer.

**Enforcement rule:** If you're adding a dependency to a component, check what layer it's in. Infrastructure containers (bottom layer) must not pull in `Expected`, `enforce`, or diagnostics.

### 6.5 Explicit Component Layer Classification

To prevent ambiguity in dependency analysis and code reviews, every header file must declare its architectural layer in the file-level Doxygen comment.

**Required format:**

```cpp
/**
 * @file ComponentName.h
 * @brief One-line summary.
 *
 * @layer Infrastructure
 *
 * [rest of description]
 */
```

**Defined layers** (in dependency order, lowest to highest):

| Layer | Description | May Depend On | Examples |
|-------|-------------|---------------|----------|
| **Infrastructure** | Core data structures, zero internal deps | `std` only | `FastHashMap`, `StableHashMap`, `SmallVector` |
| **CoreUtility** | Lightweight utilities usable everywhere | `std` + Infrastructure | `Stringify`, `DiagnosticLogger_Core`, `FloatingPointComparison`, `TypeTraits` |
| **Enforcement** | Contracts, error handling | All below | `enforce.h`, `Expected.h`, `ContractException` |
| **Policy** | Policy-based configurable components | All below | `SortedContainer`, `ConcurrencyPolicies` |
| **Application** | Full-featured components with rich dependencies | All below | `DiagnosticLogger`, `EqualityComparisons`, `Tensor` |
| **Serialization** | Format-specific I/O | All below | `FatPJson`, `FatPCbor`, `BinaryLite` |

**Rules:**

1. Components may only `#include` headers from layers **at or below** their own layer
2. Mismatch between `@layer` tag and actual includes is a **Critical** violation
3. If a component provides both pure logic and diagnostic enhancements, it must either:
   - Be placed in the higher layer (Application), OR
   - Be split into two headers (`Component_Core.h` + `Component.h`)
4. AI and human reviewers must verify the `@layer` tag matches actual dependencies

**The `_Core` Suffix Convention:**

When a component needs both a lightweight base version and a full-featured version:

| Header | Layer | Purpose |
|--------|-------|---------|
| `DiagnosticLogger_Core.h` | CoreUtility | Minimal logging hook, zero heavy deps |
| `DiagnosticLogger.h` | Application | Full logging system with sinks, formatting, etc. |

This pattern allows Infrastructure and CoreUtility components to use basic logging without pulling in the entire diagnostic subsystem.

---

## 7. General Documentation Standards

### 7.1 Required Manual Sections

| Section | Purpose |
|---------|---------|
| **What is [Component]?** | Problem statement with concrete code examples of the bad situation; survey of C++ landscape; where this component fits |
| **Core Architecture** | How it works internally (design, not just API); why decisions were made; performance characteristics explained |
| **Getting Started** | Prerequisites, integration, first complete program |
| **Feature Sections** | Comprehensive API coverage with examples; edge cases and gotchas |
| **Performance** | Benchmark methodology, test environment, results tables, interpretation |
| **Comparison** | Side-by-side tables with alternatives; code examples; clear verdicts |
| **Migration Guide** | Step-by-step from existing approaches |
| **Best Practices** | When to use (and not); naming conventions; API patterns |
| **Troubleshooting** | Common issues with symptoms/solutions; compilation errors; runtime errors |
| **Summary** | Key features; performance profile; quick start code; related components |

### 7.2 Documentation Vocabulary Rules

**These vocabulary rules apply to user-facing documentation only.**

The following terms are banned in **user manuals, guides, and overview documents** because they are vague. Replace with mechanism-specific language:

| Banned Term | Required Replacement |
|-------------|---------------------|
| Fast | Zero-allocation, O(1), cache-local |
| Safe | Bounds-verified, lifetime-tracked |
| Efficient | Constant-time, single-pass |
| Simple | Minimal API, single-header |
| Powerful | Composable, policy-based |
| Easy | No configuration required |
| Flexible | Configurable, pluggable |

**Scope clarification -- where the ban applies:**

| Content Type | Vocabulary Ban Applies? |
|--------------|------------------------|
| Teaching documents (all types in Teaching Documents Style Guide) | **Yes** |
| Doxygen comments (`/** */`, `///`) | **No** |
| Code comments (`//`) | **No** |
| Component names and identifiers | **No** |

**Rationale:** User-facing documentation shapes expectations. Vague terms like "fast" create ambiguous promises. In contrast, Doxygen comments are technical specifications where terms like "fast lookup" are understood in context, and code comments are implementation notes for maintainers. Restricting vocabulary in code comments would impede communication without benefit.

### 7.3 Documentation Philosophy

**Manuals are teaching documents, not API summaries.**

Assume the reader is intelligent but not necessarily an expert C++ programmer, and likely unfamiliar with this specific problem domain. They need to understand *what* each feature is, *why* it exists, and *when* to use it -- not just *how* to call the API. Explain concepts that experienced developers might take for granted (atomics, lock-free algorithms, ADL) without being condescending.

**Every major section should answer:**

1. **What is this?** -- Define the concept in plain terms
2. **Why does it exist?** -- What problem does it solve? What happens without it?
3. **When should I use it?** -- Decision criteria, trade-offs, alternatives
4. **How do I use it?** -- API with complete, compilable examples

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

### 7.4 Documentation Style

- **Code before explanation** -- Show, then tell
- **Explains "why" before "how"** -- Motivation before mechanics
- **Tables for comparisons** -- Easier to scan than prose
- **Teaching moments** -- Explain the "why" without being condescending
- **No jargon without purpose** -- Accessible without dumbing down
- **Explicit namespace qualification** -- Always use `fat_p::`, etc. (or local `using` directives)
- **Complete, compilable examples** -- Not fragments

**Forbidden phrases (LLM boilerplate):**

These phrases add no value and signal filler content:

| Forbidden | Why | Instead |
|-----------|-----|---------|
| "This class provides..." | States the obvious | Start with what problem it solves |
| "This component allows..." | Passive, vague | State what it *does* |
| "In this section, we will..." | Unnecessary preamble | Just start |
| "As mentioned earlier..." | Reader can scroll | Remove or link directly |
| "It is important to note that..." | If it's important, just say it | State the fact |
| "In order to..." | Verbose | Use "to" |
| "Utilize" | Pretentious | Use "use" |

**Forbidden meta-commentary:**

Do not comment on the psychological effect documentation will have on readers. Let content stand on its own merit.

| Forbidden | Why |
|-----------|-----|
| "builds trust" | Self-congratulatory; undermines the credibility it claims |
| "builds credibility" | Same problem |
| "makes readers confident" | Telling readers how to feel |
| "establishes authority" | If you have to say it, you don't have it |
| "demonstrates expertise" | Show, don't announce |

**Wrong:** "Being honest about limitations builds trust with readers."
**Right:** "Be honest about limitations." (Then actually be honest.)

### 7.5 Comparison Sections

When comparing to external libraries, **assume the reader doesn't know them**. Don't just list feature differences -- provide context:

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

### 7.6 Diagram Guidelines

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

## 8. Benchmark Environment Reference

### 8.1 Windows Test Machine

**Hardware:**

| Component | Specification |
|-----------|---------------|
| Processor | Intel Core Ultra 9 285K |
| RAM | 64.0 GB |
| GPU | NVIDIA GeForce RTX 5080 (16 GB GDDR7) |
| Storage | 1.9 TB SSD + 954 GB SSD |
| Architecture | x64 |
| OS | Windows 11 Pro |

**Debug build (MSVC 2022):**
```
/std:c++17 /Od /ZI /RTC1 /MDd /EHsc /W3 /sdl /GS
/D "_DEBUG" /D "_CONSOLE" /D "NOMINMAX" /D "WIN32_LEAN_AND_MEAN"
```

**Release build (MSVC 2022 -- for benchmarks):**
```
/std:c++17 /O2 /DNDEBUG /MD /EHsc /W3
/D "NOMINMAX" /D "WIN32_LEAN_AND_MEAN"
```

### 8.2 Linux Test Machine

**Debug build (GCC 11+):**
```bash
g++ -std=c++17 -O0 -g -Wall -Wextra -fsanitize=address,undefined
```

**Release build (GCC 11+ -- for benchmarks):**
```bash
g++ -std=c++17 -O3 -DNDEBUG -march=native -flto
```

> **Critical:** Benchmarks must always run with Release/optimized builds (`/O2` or `-O3`), never Debug builds which disable optimizations and add runtime checks.

---

## 9. Quick Reference Checklist

### Before Submitting Code:

- [ ] Compiles successfully
- [ ] No truncated code
- [ ] No AI process comments (`NEW`, `FIXED`, etc.)
- [ ] Lines wrapped at 100 columns
- [ ] No special symbols or unusual characters
- [ ] File names unchanged (unless explicitly renaming)
- [ ] Class names unchanged (unless explicitly renaming)
- [ ] Download link provided (if files modified)
- [ ] No backwards compatibility aliases
- [ ] `@layer` tag present in file header

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
- [ ] Each section explains **what**, **why**, and **when** -- not just how
- [ ] No "API summary" sections -- every feature has context and rationale
- [ ] Comparison sections introduce external libraries before comparing
- [ ] Complete, compilable examples
- [ ] Tables for comparisons
- [ ] Explicit namespace qualification (or local `using` directives)
- [ ] No `using namespace` at global scope (local scope OK)
- [ ] Diagrams where appropriate

### Before Submitting Container Documentation:

- [ ] All 6 container sections present (Intent, Invariant, Complexity, Invalidations, Concurrency, When to Use)
- [ ] Iterator invalidation table uses standard format
- [ ] Container name follows naming conventions (invariant-based, not algorithm-based)
- [ ] Error handling documented per container weight

### Before Submitting Doxygen:

- [ ] All public classes have `@brief` and `@tparam`
- [ ] All public methods have `@brief`, `@param`, `@return`
- [ ] Complexity documented with `@note Complexity: O(...)`
- [ ] Thread-safety documented with `@note Thread-safety: ...`
- [ ] No banned vocabulary in Doxygen comments
- [ ] No Doxygen on private implementation details
- [ ] File header present with `@file`, `@brief`, and `@layer`
- [ ] Code examples use `@code` / `@endcode`
- [ ] Cross-references use `@see`

---

## 10. AI Operational Behavior

This section governs how AI assistants must behave when working with Fat-P.

### 10.1 Scope

This contract governs AI behavior when:
- Generating new Fat-P components or artifacts
- Reviewing or modifying existing Fat-P code/documentation
- Answering questions about Fat-P design or implementation
- Producing any output that claims compliance with Fat-P standards

### 10.2 The Override Rule

> **If any AI output conflicts with these Development Guidelines, the AI output is invalid.**

Human maintainers may override AI concerns with explicit justification, but AI assistants must surface potential violations.

### 10.3 Non-Goals for AI Assistants

AI assistants **must not**:

1. Infer unspecified guarantees or invariants
2. Generalize behavior from `std::` equivalents unless explicitly documented
3. Invent benchmark results, performance claims, or test outcomes
4. Assume concurrency, SIMD optimization, or allocation behavior unless stated
5. Optimize for brevity at the expense of required structure
6. Use banned vocabulary without mechanism-specific replacement
7. Pretend to have compiled or executed code without actually doing so
8. Suggest "gradual adoption", "incremental migration", or "backward compatible" approaches
9. Preserve existing patterns solely because they exist
10. Weigh "disruption" as a negative when evaluating fixes

### 10.4 Required AI Behaviors

#### Capability Disclosure

Upon first receiving Fat-P guidelines, AI should be prepared to complete the **AI Capabilities Declaration** template on request. Key disclosures:

| Capability | Requirement |
|------------|-------------|
| Compilation | State honestly: Yes, No, or Limited |
| Context limits | Approximate token/file limits |
| Truncation handling | Commit to flagging, not guessing |

**If you cannot compile:** State this explicitly. This is acceptable. Do NOT claim compilation success without actual compilation.

#### Input Validation

Upon receiving guidelines, code, or documentation:

1. **Verify completeness** -- Scan for truncation signs
2. **If truncation suspected**, immediately state:
   > INPUT APPEARS TRUNCATED at [location]. Missing content: [description]. I will not proceed until provided with the complete version.
3. **List missing dependencies** required for meaningful review/generation
4. **Do NOT guess** at truncated or missing content

#### Authority Application

- Development Guidelines override all other documents
- When conflicting guidance appears, note the conflict and apply Development Guidelines
- Never weaken invariants for convenience

#### Vocabulary Enforcement

Banned terms must be replaced with mechanism-specific language in user-facing documentation (manuals, guides, overviews). See Section 7.2 for scope. Doxygen comments and code comments are exempt.

#### Layer Verification Protocol

Before flagging any dependency as a violation:

1. Check if the included header has a `@layer` tag
2. If tagged, verify the inclusion is permitted by the layer hierarchy (Section 6.5)
3. If untagged, **ask the human maintainer** for clarification before claiming violation
4. Never assume a component's layer from its name alone
5. Remember: `*_Core.h` headers are typically CoreUtility layer, not the same as the full version

**Critical:** `DiagnosticLogger_Core.h` is **CoreUtility** layer. It is NOT in the Diagnostics/Application layer. Do not flag its use as a layer violation.

#### Output Requirements

Every AI-generated artifact must:
- Follow required templates exactly
- Include mandatory honesty elements ("Where it loses", caveats)
- Use only allowed vocabulary
- Provide complete code (never truncated)
- Provide download links only for files that were actually modified (do not attach unchanged files unless explicitly requested)
- Include explicit namespace qualification

#### Deliverable Packaging Protocol (Modified Files Only)

When AI output includes downloadable files (updated headers, tests, benchmarks, or docs), it must follow this protocol so maintainers can verify what changed without guessing.

**Default mode:** Provide direct download links for each modified file.  
**Optional modes:** Provide a `.zip` or `.patch` *only when the user explicitly requests it*.

##### 1) Modified Files Manifest (MUST)
Include a section titled:

- `Modified Files (N)`

Where `N` is the number of **repo-relative** files actually modified.

Each entry must include:
- the repo-relative path (`include/Foo.h`, `tests/test_Foo.cpp`, etc.),
- a one-sentence intent ("what changed and why"),
- optionally: the primary symbols touched (for fast review).

##### 2) Links Must Match the Manifest (MUST)
- Provide download links **only** for the files in the manifest.
- Do **not** attach unchanged "context" or "dependency" files. If context is needed, quote a small snippet inline instead.
- If the user requested a `.zip`, it must contain **only** the files in the manifest and preserve their repo-relative paths.
- If the user requested a `.patch`, it must apply cleanly to the stated base revision; still include the manifest.

##### 3) Zero-Change Case (MUST)
If no files were modified:
- explicitly say **"No files were modified."**
- provide **no** download links.

##### 4) Consistency Rule (MUST)
- The `N` in `Modified Files (N)` must match the number of modified-file links provided.
- Do not provide multiple links for the same file unless the user asked for multiple formats.

##### Example (correct)
```text
Modified Files (3)
1) include/FatPJson.h -- Fix 32-bit length narrowing in read_string().
2) tests/test_FatPJson.cpp -- Add regression test for oversized length field.
3) docs/FatPJson_User_Manual.md -- Document the new error behavior.

Downloads (modified files only)
- [include/FatPJson.h](<link>)
- [tests/test_FatPJson.cpp](<link>)
- [docs/FatPJson_User_Manual.md](<link>)
```

##### Example (incorrect)
- Providing a `.zip` containing 200 files when only 3 were modified.
- Providing links to unchanged files "for completeness".

#### Compilation and Execution Honesty

| If You Can Compile | Requirement |
|--------------------|-------------|
| **Yes** | Only claim success if you actually executed the compiler |
| **No** | State: "I cannot execute code; analysis is static only" |

Never claim compilation success without tool invocation evidence.

### 10.5 Failure Mode Expectations

When uncertain or constrained:

1. **Ask for clarification** rather than guess
2. **Prefer "unknown"** over inference
3. **Flag potential violations** explicitly
4. **Request missing inputs** before proceeding

**Example responses:**

> "The provided file appears truncated at line X. I cannot complete the review until provided with the complete version."

> "This usage assumes thread-safety, but the component documentation states NOT thread-safe. This violates the guarantee explicitness requirement."

> "I cannot verify this compiles; my analysis is static only."

### 10.6 Reset Protocol

**Resets Are Human-Initiated Only**

AI assistants must **never** auto-reset or suggest automated resets. Resets are a human verification tool.

**How Humans Initiate Reset:**

- Start a new conversation, OR
- Say: "Disregard all prior analysis. Review fresh with only the provided files."

**Completeness Criterion:**

> After a human-initiated reset, if any AI produces essential new findings, the artifact is not complete.

AI should expect reset reviews and should not anchor on prior conclusions.

### 10.7 AI Review Checklist

Before submitting any Fat-P artifact, AI should verify:

- [ ] No banned vocabulary in user-facing documentation (manuals, guides, overviews)
- [ ] All required template sections present
- [ ] Caveats/limitations section included
- [ ] Complete code (no truncation)
- [ ] If files were modified: a `Modified Files (N)` list is present
- [ ] Download links include **only** those `N` modified files (no extras)
- [ ] Namespace qualification explicit
- [ ] Compilation claims backed by actual execution (if claimed)
- [ ] No inference of unspecified behavior
- [ ] `@layer` tag verified against actual includes
- [ ] No "backward compatibility" suggestions

### 10.8 AI Code Review Standards

This section governs AI behavior when reviewing Fat-P code for errors, improvements, or compliance.

#### Evidence Requirements

**Every finding must include an Evidence line quoting the relevant symbol/include from the provided text. If you cannot cite evidence from the provided text, put it in Hypotheses and do not propose patches.**

Findings are classified into two categories:

| Category | Requirement | Allowed Output |
|----------|-------------|----------------|
| **Grounded** | Evidence quote + counterexample | Bug report, patch, priority rating |
| **Hypothesis** | Clearly labeled speculation | Question for human, no patch |

**Ungrounded claims that remain unevidenced after review must be removed entirely.**

#### The Five Review Rules

1. **Evidence rule**: Every bug/claim requires an Evidence line with a verbatim quote from the provided files (e.g., an `#include` line, a symbol definition, or a code snippet demonstrating the issue).

2. **Counterexample rule**: Every bug report requires a concrete counterexample demonstrating the failure:
   - **Wrong**: "This could fail when..."
   - **Right**: "Given input X, output is Y, expected Z"

3. **No unverifiable claims**: Do not claim to have compiled, executed, or tested code unless you show the actual tool invocation and output. Statements like "I verified this compiles" or "60+ tests pass" without evidence are disqualifying.

4. **Semantic preservation**: Patches must not change observable behavior for cases that currently work correctly. If a fix changes semantics (not just performance), explicitly document what changes and justify why.

5. **No unevidenced components**: Any referenced header/class/function must be supported by:
   - A verbatim snippet from the provided files, OR
   - An explicit user-provided list of files/components
   
   If neither exists, the claim must be labeled as speculation and kept out of bug lists and patch lists.

#### Dependency and Layer Claims

Before asserting a missing include or layer violation:

1. **Show the include chain** -- If claiming a transitive include is missing, demonstrate the actual chain (e.g., `A.h` -> `B.h` -> `C.h`)
2. **Check `@layer` tags** -- Use the header's declared layer, not assumptions based on naming
3. **Quote the violation** -- Show the specific `#include` line that violates the layer hierarchy

**Example of proper evidence:**

> **Evidence:** Line 21 includes `DiagnosticLogger_Core.h`. Per its `@layer CoreUtility` tag (line 5 of that header), this is permitted for Application-layer components.

#### Counterexample Format

Bug reports must follow this structure:

```
**Bug:** [Brief description]
**Evidence:** [Verbatim code quote showing the issue]
**Counterexample:**
  - Input: [concrete values]
  - Actual: [what the code produces]
  - Expected: [what it should produce]
**Impact:** [Severity and consequences]
**Fix:** [Proposed solution]
```

**Example:**

```
**Bug:** unordered_multiset comparison produces false positives
**Evidence:** Lines 444-474 use `b.find(keyA)` without tracking matched elements
**Counterexample:**
  - Input: a = {1, 1, 1}, b = {1, 2, 3}
  - Actual: areEqual returns true (find(1) succeeds for all three elements in a)
  - Expected: false (b contains 2 and 3 which are not in a)
**Impact:** P0 -- incorrect equality results for multi-containers
**Fix:** Track matched elements via consume-matching algorithm with equal_range()
```

#### Prohibited Review Behaviors

AI reviewers must not:

1. **Fabricate components** -- Do not analyze or patch components that don't exist in the provided files
2. **Claim false consensus** -- Do not assert "as we established" or "as confirmed" without a direct quote
3. **Template-match** -- Do not apply generic review patterns (e.g., "all equality code needs EqualityAny") without evidence they apply to this specific codebase
4. **Accumulate speculation** -- Do not let ungrounded hypotheses influence subsequent analysis or patches

#### Multi-AI Review Protocol

When multiple AI reviewers analyze the same code:

1. **Independent analysis first** -- Each AI reviews without seeing others' output
2. **Cross-validation** -- Human or lead AI identifies agreements and conflicts
3. **Evidence arbitration** -- Conflicting claims are resolved by which has stronger evidence, not by vote
4. **Hallucination detection** -- Claims that appear in one AI's output but have no evidence in the source files should be flagged and discarded

**Red flags indicating unreliable AI output:**

- References to components not in the provided file set
- Claims of compilation/testing without tool output
- Assertions about "standard practice" or "typical patterns" without codebase evidence
- Patches that solve problems not demonstrated by counterexample

---

## Changelog

### v2.8 (January 2026)
- Expanded Section 4.2 Formatting Standards with complete clang-format configuration
- Updated line width policy: 100 columns typical, 120 columns absolute maximum
- Added PenaltyExcessCharacter mechanism to discourage lines over 100 columns
- Added comprehensive clang-format options for all formatting aspects
- Added key formatting rules summary table

### v2.7 (January 2025)
- Added Systemic Hygiene Policy to governance document set (6 documents total, up from 4)
- Added Benchmark Code Style Guide to governance table
- Added cross-reference in Section 4.9 to Systemic Hygiene Policy for namespace rules
- Added "How do I write a benchmark?" and "Can these headers be included together?" questions to document selection table

### v2.6 (December 2025)
- Consolidated teaching document governance from 5 separate guides to 1 unified guide
- Removed: Overview Style Guide, User Manual Style Guide, Companion Guide Style Guide, Case Study Style Guide, Benchmark Results Style Guide
- Added: Teaching Documents Style Guide (covers all 9 document types: Overview, User Manual, Companion Guide, Case Study, Foundations, Handbook, Pattern Guide, Design Note, Benchmark Results)
- Updated Document Governance table: reduced from 7 to 4 governance documents
- Updated "Which document do I write?" table: expanded to cover all 9 teaching document types
- Updated Section 7.2 scope clarification table to reference unified Teaching Documents Style Guide

### v2.5 (December 2025)
- Added deliverable packaging protocol to Section 10.4 (Modified Files manifest + link consistency rules)
- Updated Section 10.7 AI Review Checklist to require modified-file-only downloads

### v2.4 (December 2025)
- Added Section 10.8: AI Code Review Standards with evidence requirements, counterexample format, and multi-AI review protocol
- Added The Five Review Rules for AI code review (evidence, counterexample, no unverifiable claims, semantic preservation, no unevidenced components)
- Added Prohibited Review Behaviors to prevent hallucination and template-matching
- Added Multi-AI Review Protocol for cross-validation and hallucination detection

### v2.3 (December 2025)
- Clarified Section 7.2: Vocabulary ban applies only to user-facing documentation (manuals, guides, overviews)
- Updated Section 4.7.5: Doxygen comments are exempt from vocabulary ban; precise language is recommended but not required
- Updated Section 10.4: AI vocabulary enforcement limited to user-facing documentation
- Updated Section 10.7 checklist: Clarified vocabulary scope

### v2.2 (December 2025)
- Added Section 1.3: Explicit prohibition on "backward compatible" and "incremental adoption" thinking
- Added Section 6.5: Explicit Component Layer Classification with mandatory `@layer` tag
- Expanded Section 6.4: Clarified CoreUtility vs Application layers; documented `_Core` suffix convention
- Updated Section 4.7.8: File headers now require `@layer` tag
- Added Section 10.4: Layer Verification Protocol for AI assistants
- Updated Section 10.3: Added items 8-10 prohibiting backward compatibility suggestions
- Updated Section 9 checklist: Added `@layer` tag verification

### v2.1 (December 2025)
- Clarified Section 4.3: Split member variable convention into instance (`m` prefix) and static (`s` prefix)
- Clarified Section 4.3: Split constants into preprocessor (`SCREAMING_SNAKE`) and `constexpr` (`k` prefix)
- Expanded Section 4.5: Added detailed anonymous namespace justification requirements with placement, format, and examples

### v2.0 (December 2025)
- Consolidated from 9 documents to 7 documents
- Merged Doxygen Style Guide into Section 4.7
- Merged AI Operational Contract into Section 10
- Removed Governance README (redundant with Document Governance section)
- Removed Benchmark Style Guide references
- Fixed Unicode characters throughout
- Clarified vocabulary ban applies to documentation prose, not component names
- Clarified function naming is camelCase (not snake_case)
- Expanded adjective table (Section 2.1) with 11 additional entries
- Updated namespace naming convention to "lowercase" (removed "with underscores")
- Added forbidden meta-commentary rule (Section 7.4): no "builds trust" language
- Updated Human Guidance to include AI Capability Estimates as appendix

### v1.1 (December 2025)
- Added Section 1.4: Policy-Based Design (clarified as optional)
- Added Section 1.5: Separation of Concerns principle
- Added Section 4.3: Member naming rationale for `mPrefix` convention
- Updated Section 4.5: Softened anonymous namespace rule from "forbidden" to "strongly discouraged"
- Added Section 4.6.1: Doxygen Style Reference
- Updated Document Governance table: Added Doxygen Style Guide and AI Operational Contract
- Added standardized thread-safety phrases to Section 6.1

### v1.0 (Initial)
- Original 7-document governance suite

---

*Fat-P Library Development Guidelines v2.8 -- January 2026*
