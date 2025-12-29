# FAT-P Teaching Documents Style Guide (Enhanced Edition)
## Complete specifications for **Overviews**, **User Manuals**, **Companion Guides**, **Case Studies**, **Foundations**, **Handbooks**, **Pattern Guides**, **Design Notes**, and **Benchmark Results**
*(Designed for humans **and** AI retrieval/reasoning clients.)*

---

## Purpose

FAT-P "teaching documents" exist to make readers better at:

- **C++ semantics** (UB, object lifetime, `noexcept`, optimizer models, etc.)
- **systems constraints** (allocators, cache locality, branch prediction, SIMD, ABI)
- **engineering discipline** (performance invariants, API contracts, policy design)

…and to do so using FAT-P as a concrete specimen: real code, real tradeoffs, real guarantees.

This guide defines:
- the document **taxonomy** (what type to write)
- mandatory **structure** for each type (complete detail, even with repetition)
- **AI-first** constraints (metadata, headings, labeling)
- **evidence standards** and "verbatim vs pseudocode" discipline
- templates and checklists

---

## Document Taxonomy

Use this table to pick the correct document type.

| Doc type | Primary question it answers | When to use | Evidence level |
|---|---|---|---|
| **Overview** | "What is this component?" | Reader is new; wants orientation + positioning | Low (descriptive) |
| **User Manual** | "How do I use this correctly?" | Integration, recipes, pitfalls, troubleshooting | Medium (examples + constraints) |
| **Companion Guide** | "Why is it designed this way?" | Rationale, tradeoffs, philosophy, domain story | Medium–High (links to evidence) |
| **Case Study** | "Why did this fail, and how do I fix it?" | One sharp failure mode, one fix, with proof | High (audit/bench/sanitizers) |
| **Foundations** | "What background do I need to reason about this?" | Semantics/history/mental models | Medium (citations encouraged) |
| **Handbook** | "What discipline should teams adopt?" | Methods, invariants, checklists | Medium (frameworks + examples) |
| **Pattern Guide** | "How do I apply this pattern?" | Reusable design/implementation recipe | Medium (reference implementation) |
| **Design Note** | "What decision did we make, and why?" | Narrow decision record; precursor to later doc | Medium (options + decision) |
| **Benchmark Results** | "How does this perform?" | Structured presentation of benchmark data | High (measured data) |

---

## Naming Conventions

### File Names

**Format:** `<Doc Type> - <Title>.md`

Examples:
- `Overview - StableHashMap.md`
- `Case Study - The Noreturn Mirage and the Noexcept Cliff.md`
- `Foundations - C++ Historical Context.md`
- `Pattern Guide - The Factory Pattern Done Right.md`
- `Benchmark Results - SmallVector.md`

**Filename rules:**
- Avoid `:` `/` `\` backticks, and OS-sensitive punctuation
- Prefer hyphens over slashes: `Stack-Heap-Cache` not `Stack/Heap/Cache`
- Keep ≤ ~80 characters if possible

### Internal H1 Title

The H1 **must match the filename prefix**:

`# <Doc Type> - <Original Title>`

This allows both humans and AI to reliably identify the document type from the content alone.

---

## AI Scaffolding: Two Layers

Every FAT-P teaching document has two layers of scaffolding: machine-readable metadata (hidden) and visible structure (rendered). Both are required.

---

### Layer 1: YAML Front Matter (Machine-Readable)

The YAML block at the top of every document is **hidden when rendered** but **parsed by AI systems** for retrieval, cross-referencing, and semantic search.

```yaml
---
doc_id: CS-SMALLVECTOR-001
doc_type: "Case Study"
title: "Multi-Index Design, Stack/Heap/Cache, and Why SmallVector Wins"
fatp_components: ["SmallVector"]
topics: ["tensor indexing", "allocator overhead", "small buffer optimization", "cache locality"]
constraints: ["heap allocation in hot loops", "memory hierarchy", "temporary containers"]
cxx_standard: "C++17"
build_modes: ["Debug", "Release"]
last_verified: "2025-12-27"
audience: ["C++ developers", "AI assistants"]
status: "reviewed"
---
```

#### Field Reference

| Field | Required | Purpose | How to Choose Values |
|-------|----------|---------|---------------------|
| `doc_id` | Yes | Unique identifier for cross-referencing | Format: `{TYPE}-{COMPONENT}-{NNN}`. Types: OV (Overview), UM (User Manual), CG (Companion Guide), CS (Case Study), FN (Foundations), HB (Handbook), PG (Pattern Guide), DN (Design Note), BR (Benchmark Results) |
| `doc_type` | Yes | Tells AI which template/expectations apply | Must match one of: "Overview", "User Manual", "Companion Guide", "Case Study", "Foundations", "Handbook", "Pattern Guide", "Design Note", "Benchmark Results" |
| `title` | Yes | Human-readable title for search results | Match the H1 title exactly (without markdown formatting) |
| `fatp_components` | Yes | Links document to code | List FAT-P header names without `.h` extension. Use `[]` if no specific component |
| `topics` | Yes | Semantic search keywords | 3-8 specific terms. Prefer noun phrases over verbs. Include both problem terms ("heap allocation") and solution terms ("small buffer optimization") |
| `constraints` | Yes | Engineering constraints addressed | What hardware/language/design constraints does this document explain? Examples: "cache effects", "exception boundaries", "UB rules", "memory hierarchy" |
| `cxx_standard` | Yes | Minimum C++ version | "C++17", "C++20", or "C++23" |
| `build_modes` | If relevant | Debug/Release behavior differences | Include if behavior changes under `NDEBUG`, sanitizers, or optimization levels |
| `last_verified` | Yes | When code/claims were tested | ISO date format: YYYY-MM-DD. Update when re-validating code excerpts |
| `audience` | Yes | Who should read this | Always include "AI assistants". Add specific roles: "C++ developers", "library maintainers", "performance engineers" |
| `status` | Yes | Document maturity | "draft" (incomplete), "reviewed" (peer-reviewed), "final" (stable) |

#### Choosing Good Topics

Topics enable semantic search. Choose terms that someone would search for when they have this problem.

**Good topics:** Specific, searchable, problem-oriented
```yaml
topics: ["heap allocation in loops", "small buffer optimization", "tensor indexing", "cache locality"]
```

**Bad topics:** Vague, too broad, not searchable
```yaml
topics: ["performance", "memory", "optimization", "C++"]
```

#### Choosing Good Constraints

Constraints describe the engineering forces that shaped the solution. They answer "why is this hard?"

**Good constraints:** Name specific hardware/language/design limitations
```yaml
constraints: ["malloc overhead in hot paths", "L1 cache line size", "temporary object lifetime"]
```

**Bad constraints:** Vague or just restating the problem
```yaml
constraints: ["performance issues", "memory problems", "slow code"]
```

---

### Layer 2: Visible Scaffolding (Human + Machine)

These elements are **rendered for human readers** and also parsed by AI for structure understanding.

#### Required Elements (All Document Types)

| Element | Purpose | Format |
|---------|---------|--------|
| **H1 Title** | Document identification | `# {Doc Type} - {Title}` — must match YAML `title` |
| **Scope** | What is covered | 1-3 sentences or short paragraph |
| **Not covered** | Explicit exclusions | Bullet list of what this doc does NOT address |
| **Prerequisites** | Reader assumptions | Bullet list of knowledge assumed |
| **Table of Contents** | Navigation | Required for docs > 800 lines; anchor links to all major sections |

#### Document Type Cards

Each document type has a **Card** — a quick-reference summary in a consistent format. Cards enable fast scanning and AI extraction of key facts.

##### Case Study Card
```markdown
## Case Study Card
**Problem:** [One sentence describing what went wrong]  
**Constraint:** [The hardware/language constraint that caused it]  
**Symptom:** [What the user observed]  
**Root cause:** [The actual bug/issue]  
**Fix pattern:** [How to fix it]  
**FAT-P components used:** [Components involved]  
**Build-mode gotchas:** [Debug/Release differences, or "None"]  
**Guarantees:** [What the fix guarantees]  
**Non-guarantees:** [What the fix does NOT guarantee]
```

##### Handbook Card
```markdown
## Handbook Card
**Domain:** [What discipline this covers]  
**Core principle:** [One-sentence philosophy]  
**Key discipline:** [Primary practice taught]  
**Common failure:** [What goes wrong without this discipline]  
**Hard rules:** [Non-negotiable requirements]  
**Applies to:** [Scope of applicability]  
**Build-mode notes:** [Relevant build considerations]  
**Guarantees:** [What following this discipline guarantees]  
**Non-guarantees:** [What it does NOT guarantee]
```

##### Overview Card
```markdown
## Overview Card
**Component:** [Name]  
**Problem solved:** [One sentence]  
**When to use:** [Conditions favoring this component]  
**When NOT to use:** [Conditions favoring alternatives]  
**Key guarantee:** [Primary invariant or property]  
**Alternatives:** [std:: equivalent, competitors]  
**Read next:** [Suggested follow-up documents]
```

##### Companion Guide Card
```markdown
## Companion Guide Card
**Component:** [Name]  
**Design question:** [The "why" question this answers]  
**Key tradeoff:** [Primary tension in the design]  
**Decision made:** [What was chosen]  
**Rejected alternatives:** [What was NOT chosen, and why]  
**Historical context:** [Era/constraints that shaped the decision]
```

##### Pattern Guide Card
```markdown
## Pattern Guide Card
**Pattern:** [Name]  
**Problem:** [What problem this pattern solves]  
**Solution shape:** [One-sentence structural description]  
**When to use:** [Conditions favoring this pattern]  
**When NOT to use:** [Conditions favoring alternatives]  
**FAT-P implementation:** [How FAT-P implements this pattern]  
**Key insight:** [The non-obvious thing that makes it work]
```

##### Foundations Card
```markdown
## Foundations Card
**Topic:** [What background this covers]  
**Why it matters:** [Connection to FAT-P usage]  
**Key concepts:** [3-5 main ideas introduced]  
**Mental model:** [The way to think about this]  
**Common misconceptions:** [What people get wrong]  
**Read next:** [Documents that build on this foundation]
```

##### User Manual Card
```markdown
## User Manual Card
**Component:** [Name]  
**Primary use case:** [Most common scenario]  
**Integration pattern:** [How to add to existing code]  
**Key API:** [Most important functions/types]  
**Common mistakes:** [What to avoid]  
**Performance notes:** [What to know about performance]
```

##### Design Note Card
```markdown
## Design Note Card
**Decision:** [What was decided]  
**Context:** [Why this decision was needed]  
**Options considered:** [Alternatives evaluated]  
**Chosen option:** [What was selected]  
**Rationale:** [Why this option won]  
**Implications:** [What this means for users/maintainers]
```

##### Benchmark Results Card
```markdown
## Benchmark Results Card
**Component:** [What was benchmarked]  
**Competitors:** [What it was compared against]  
**Key finding:** [One-sentence summary]  
**Conditions:** [Platform, compiler, flags]  
**Caveats:** [Limitations of the results]  
**Raw data:** [Link to appendix or file]
```

#### Key Takeaway Card (Optional but Recommended)

For long documents, include a summary table after the Card:

```markdown
## Key Takeaway Card

| Principle | One-Line Summary |
|-----------|-----------------|
| **[Name]** | [One sentence] |
| **[Name]** | [One sentence] |
| ... | ... |
```

---

### Scaffolding Checklist

Before publishing any teaching document:

#### YAML Front Matter
- [ ] Begins with `---` fence
- [ ] `doc_id` follows naming convention
- [ ] `doc_type` matches document structure
- [ ] `title` matches H1 exactly
- [ ] `fatp_components` lists relevant headers
- [ ] `topics` has 3-8 specific, searchable terms
- [ ] `constraints` names engineering forces
- [ ] `last_verified` is current
- [ ] `audience` includes "AI assistants"
- [ ] `status` reflects actual maturity
- [ ] Ends with `---` fence

#### Visible Structure
- [ ] H1 title matches YAML and follows `# {Type} - {Title}` format
- [ ] Scope section present
- [ ] Not covered section present (as bullet list)
- [ ] Prerequisites section present (as bullet list)
- [ ] Document-type Card present with all required fields
- [ ] Table of Contents present (if > 800 lines)
- [ ] Key Takeaway Card present (recommended for long docs)

---

### Complete Scaffolding Example

```markdown
---
doc_id: CS-HASHMAP-002
doc_type: "Case Study"
title: "The Case of the Slow Miss"
fatp_components: ["StableHashMap", "FatPBenchmarkUtils"]
topics: ["hash table", "miss performance", "SIMD filtering", "empty slot detection", "Swiss Table"]
constraints: ["cache misses", "SIMD mask operations", "probe sequence termination"]
cxx_standard: "C++17"
build_modes: ["Release"]
last_verified: "2025-12-27"
audience: ["C++ developers", "AI assistants", "performance engineers"]
status: "reviewed"
---

# Case Study - The Case of the Slow Miss

## Scope
This case study examines a 3× performance regression in hash table miss detection 
and the counter-based investigation that identified the root cause.

## Not covered
- Hash table design decisions (see Companion Guide - StableHashMap Design History)
- General benchmarking methodology (see Handbook - Performance Engineering Methodology)
- SIMD intrinsics tutorial

## Prerequisites
- Basic understanding of hash table probe sequences
- Familiarity with SIMD concepts (match masks, population count)
- Awareness of cache effects on performance

## Case Study Card
**Problem:** Miss lookups 3× slower than expected  
**Constraint:** SIMD match mask includes slots past first empty  
**Symptom:** 10.5 ns miss latency vs expected 3-4 ns  
**Root cause:** Scanning all occupied slots instead of stopping at first empty  
**Fix pattern:** Filter match mask by empty mask before iteration  
**FAT-P components used:** StableHashMap, FatPBenchmarkUtils  
**Build-mode gotchas:** None  
**Guarantees:** Miss performance O(1) expected with minimal key comparisons  
**Non-guarantees:** Does not guarantee hit performance improvement

## Table of Contents
- [The Symptom](#the-symptom)
- [Adding Counters](#adding-counters)
- [The Theory](#the-theory)
- [The Fix](#the-fix)
- [Results](#results)

---

[Document content follows...]
```

---

### Why Scaffolding Matters

#### For Humans
- Consistent structure reduces cognitive load
- Cards enable fast scanning without reading full document
- Prerequisites prevent wasted time on wrong documents
- Not covered prevents false expectations

#### For AI Systems
- YAML enables semantic search across document corpus
- Consistent structure enables reliable extraction
- Topics and constraints enable matching user questions to relevant documents
- Cards provide condensed facts for RAG retrieval
- Cross-references via `doc_id` enable document graph traversal

#### For Maintenance
- `last_verified` tracks staleness
- `status` communicates reliability
- `fatp_components` links docs to code changes
- Consistent format enables automated validation

---

## Universal Vocabulary Standards

### Banned Terms and Replacements (All Document Types)

| Banned | Replacement | Rationale |
|--------|-------------|-----------|
| Fast | Zero-allocation, O(1), cache-local, branchless | Explain the mechanism |
| Efficient | Constant-time, amortized O(1), single-pass | Specify complexity |
| Safe | Bounds-verified, lifetime-tracked, type-checked | Specify what's checked |
| Simple | Minimal API, single-header, no configuration | Specify the simplicity |
| Powerful | Composable, policy-based, extensible | Specify the capability |
| Easy | Minimal configuration, self-contained | Avoid condescension |
| Flexible | Configurable, pluggable, adapter-compatible | Specify extension point |
| Handles errors | Propagates, traps, returns Expected, terminates | Specify the mechanism |
| Thread-safe | Lock-free, mutex-protected, thread-local | Specify the strategy |
| Modern | C++17, constexpr-evaluated, SFINAE-free | Specify the standard |
| Polyfill | Wrapper, shim, compatibility layer | Web-centric; wrong domain |
| Backport | Architectural superset, enhanced implementation | Implies temporary |
| Similar to | Inspired by, extends beyond | Implies mere equivalence |

**Scope clarification:** These bans apply to documentation prose, not component names. `FastHashMap` is a valid identifier.

### No Meta-Commentary (All Document Types)

Do not comment on the psychological effect documentation will have on readers. Let content stand on its own merit.

**Forbidden phrases:**
- "builds trust"
- "builds credibility"
- "makes readers confident"
- "establishes authority"
- "demonstrates expertise"

**Wrong:** "Acknowledging limitations builds trust with readers."
**Right:** "Acknowledge limitations."

---

## Universal Structural Elements

See [AI Scaffolding: Two Layers](#ai-scaffolding-two-layers) for the complete specification of required structural elements including:
- YAML front matter
- Scope / Not covered / Prerequisites
- Document-type Cards
- Table of Contents
- Key Takeaway Card

Additional universal requirements:

### Glossary

For documents > ~1000 lines or those introducing significant terminology, include a **Glossary** section (typically as an appendix) defining key terms.

### Universal Evidence Standards

**Rule: Never invent evidence.** If you give numbers, back them with:
- benchmark output
- profiler snapshots
- sanitizer output
- or a clearly defined theoretical count ("N allocations because…")

If unproven: label as **Hypothesis**.

### Debug vs Release Must Be Explicit

If behavior changes under `NDEBUG`, `noexcept`, LTO, sanitizer builds, etc., include a callout:

> **Critical: Debug vs Release** — …

### Verbatim vs Excerpt vs Pseudocode Discipline

Readers (and AIs) trust the doc only if code excerpts are labeled precisely.

- **Verbatim**: label file + symbol, no editorial comments inside the code block
- **Annotated excerpt**: label as annotated; editorial notes clearly marked
- **Pseudocode**: label as pseudocode; never use it to justify forensic conclusions

---

# Document Type Specifications

---

# 1. OVERVIEW

## Intent

Provide **orientation and positioning**. A reader should understand:
- what the component is
- what problem it solves
- when to use it (and when not to)
- how it compares to alternatives
- what to read next

An Overview is the **front door** to a component. It should position FAT-P as **architecturally superior** to both standard library equivalents and competing implementations — not merely as a temporary compatibility shim.

## The Three-Note Checklist

Every Overview MUST hit these three notes:

### 1. Permanence
This is NOT a temporary fix until compiler upgrades arrive. This IS the solution.

**Wrong:** "Until C++23 is available, use Expected..."
**Right:** "Expected provides policy-based error handling that std::expected will never offer."

### 2. Specialization
The standard is generic; FAT-P is HPC-tuned.

**Wrong:** "Similar to std::vector but with inline storage."
**Right:** "Transforms allocation-bound loops into compute-bound operations through stack-local storage."

### 3. Control
The standard is one-size-fits-all; FAT-P is policy-based.

**Wrong:** "Handles overflow safely."
**Right:** "Allows compile-time selection of overflow behavior — throw, saturate, or return Expected — without virtual dispatch overhead."

## Required Sections

### 1. Executive Summary (3-4 sentences)

**Formula:**
1. What it is (one sentence)
2. The architectural advantage (one sentence)
3. Key differentiator from standard/alternatives (one sentence)
4. Quantified benefit if available (one sentence)

**Example:**
> SmallVector is a hybrid stack/heap container that eliminates heap allocation for small element counts. Unlike naive implementations using boolean flags, it employs pointer-discriminating storage to achieve branchless element access. This architectural choice — inspired by LLVM but dependency-free — transforms allocation-bound loops into compute-bound operations, delivering 2-10x speedup for small collections.

### 2. The Problem Domain

**Structure:**
- **"What Goes Wrong Without It"** — Show broken/naive code
- **Problem table** with **Impact** column (not just "Issue")
- **"The Standard's Limitation"** — Why std:: doesn't solve it

**Key principle:** Don't just say what's wrong. Say WHY it hurts HPC workloads specifically.

### 3. Architecture

**Must include:**
- The **mechanism** that enables the performance (not just "it's fast")
- Memory layout diagram or description if applicable
- Complexity guarantees with Big-O

**Example (good):**
> SmallVector uses pointer arithmetic to distinguish inline from heap storage. When `data() == inline_buffer_`, elements are on the stack. This single pointer comparison replaces the boolean flag + branch that naive implementations require on every access.

**Example (bad):**
> SmallVector stores small arrays inline to avoid allocation.

### 4. Feature Inventory

Numbered subsections with code examples. Each feature must answer:
1. What does it do?
2. How does it achieve zero overhead?
3. When would you use it vs. alternatives?

### 5. Why Not Alternatives? (Critical Section)

Structure as exclusionary criteria:

| If You Need... | Why Not [Alternative] | FAT-P Advantage |
|----------------|----------------------|-----------------|
| Zero dependencies | LLVM SmallVector requires LLVM headers | Single header, STL only |
| Standard allocators | Boost.Container uses custom allocator model | std::allocator compatible |
| No metaprogramming | Folly uses heavy template machinery | Minimal template instantiation |

**Key insight:** Position FAT-P as the ONLY option when combining multiple requirements.

### 6. The "Forever Stuck" Reality

Address compiler lock-in explicitly:

> **Compiler Reality Check:** Scientific clusters often run RHEL 7/8 with GCC 7.x for driver compatibility. Even when C++23 offers similar features, your codebase may be contractually locked to C++17 for years. FAT-P bridges this gap permanently — not as a temporary shim, but as an architecturally superior solution that remains valuable even after compiler upgrades.

### 7. Performance Characteristics

**Must include:**
- Specific mechanisms (not just "fast")
- Benchmark methodology notes
- **Where FAT-P loses**

**Example:**
> **Where FAT-P loses:** For collections consistently exceeding inline capacity, std::vector with reserve() matches performance. SmallVector's advantage is statistical — it wins when most instances stay small.

### 8. Integration Points

Show how this component connects to the FAT-P ecosystem:

```
SmallVector
    → uses
enforce.h (bounds checking)
    → used by
Signal.h (zero-alloc slot storage)
FatPJsonLite.h (inline JSON arrays)
```

### 9. Final Assessment

**Formula:**
1. Restate the architectural advantage
2. List the three pillars: Permanence, Specialization, Control
3. One-sentence verdict

**Example:**
> SmallVector delivers on the FAT-P promise:
> 1. **Permanence:** Not a shim for std::inplace_vector — an architectural superset with policy control
> 2. **Specialization:** Cache-local iteration, zero heap allocation for small N
> 3. **Control:** Configurable inline capacity, standard allocator support
>
> For small, hot collections in allocation-sensitive code, SmallVector is the solution.

## Power Phrases for Overviews

Use these to describe FAT-P advantages:
- "Architectural superset of the standard feature"
- "Production-hardened implementation"
- "Zero-overhead abstraction"
- "Compile-time policy resolution"
- "Pointer-discriminating storage"
- "Deterministic memory behavior"
- "No virtual dispatch overhead"
- "Transforms [X]-bound code to [Y]-bound code"

## Overview Template

```markdown
---
doc_id: OV-0000
doc_type: "Overview"
title: "[Component Name]"
fatp_components: ["[Component]"]
topics: ["..."]
constraints: ["..."]
cxx_standard: "C++17"
last_verified: "YYYY-MM-DD"
audience: ["C++ developers", "AI assistants"]
status: "draft"
---

# Overview - [Component Name]

## Executive Summary

[Component] is [what it is in one clause] that [architectural advantage]. Unlike [standard/naive approach], it [key mechanism]. This [quantified benefit or architectural distinction].

---

## The Problem Domain

### What Goes Wrong Without It

```cpp
// The naive/standard approach
[Code showing the problem]
```

| Issue | HPC Impact |
|-------|------------|
| [Problem 1] | [Why it hurts HPC specifically] |
| [Problem 2] | [Why it hurts HPC specifically] |

### The Standard's Limitation

[Why std:: or C++2x doesn't solve this permanently]

---

## Architecture: [Key Mechanism Name]

```cpp
// Core type or key abstraction
[Simplified architecture code]
```

**The Mechanism:** [Explain HOW it achieves zero overhead, not just WHAT it does]

---

## Feature Inventory

### 1. [Feature Name]

[Description focusing on mechanism and zero-overhead nature]

```cpp
[Code example showing the WHY, not just syntax]
```

---

## Why Not Alternatives?

| If You Need... | Why Not [Alt] | FAT-P Advantage |
|----------------|---------------|-----------------|
| [Requirement] | [Alt's limitation] | [FAT-P solution] |

---

## The "Forever Stuck" Reality

[Address compiler lock-in: RHEL, CUDA drivers, contractual C++ version limits]

---

## Performance Characteristics

| Operation | Complexity | Mechanism |
|-----------|------------|-----------|
| [Op] | O(...) | [How it achieves this] |

### Where FAT-P Wins
[Specific scenarios]

### Where FAT-P Loses
[When alternatives match or beat FAT-P]

---

## Integration Points

```
[Component]
    → uses
[Dependency 1]
    → used by
[Dependent component 1]
```

---

## Final Assessment

[Component] delivers on the FAT-P promise:

1. **Permanence:** [Why this isn't just waiting for C++2x]
2. **Specialization:** [HPC-specific advantage]
3. **Control:** [Policy-based customization]

[One-sentence architectural verdict]

---

*[Component].h — FAT-P Library*
```

## Overview Checklist

- [ ] YAML header present
- [ ] H1 matches filename prefix
- [ ] No instances of "polyfill" or "backport"
- [ ] Executive summary states mechanism, not just benefit
- [ ] Three-Note Checklist (Permanence, Specialization, Control) addressed
- [ ] "Why Not Alternatives?" section uses exclusionary criteria
- [ ] Performance section acknowledges where FAT-P loses
- [ ] Compiler lock-in reality acknowledged
- [ ] Integration points documented
- [ ] Code examples show WHY, not just syntax
- [ ] Active voice throughout
- [ ] Specific numbers where possible (not "fast" but "2-10x")

## Common Failure Mode

An "overview" that becomes a tutorial or an argument. If it has long recipes, it's a Manual. If it argues design philosophy at length, it's a Companion Guide.

---

# 2. USER MANUAL

## Intent

Make a user successful in production:
- correct usage
- correct configuration
- correct failure handling
- common pitfalls
- migration from alternatives

The User Manual is a **book**, not a reference card. It:
- Tells the story of the problem domain
- Explains concepts deeply before showing API
- Teaches the reader to think correctly about the component
- Provides complete, production-ready examples
- Anticipates mistakes and explains how to avoid them

## The Litmus Test

After reading the User Manual, the reader should be able to:
1. Explain to a colleague why this component exists
2. Choose the right method for their use case without guessing
3. Debug common problems without external help
4. Migrate from std:: equivalents confidently

## Required Sections

### 1. Opening Story (The Hook)

Start with the problem domain, not the API. Tell the reader *why this thing exists*.

**Pattern:**
```markdown
## The Hash Table Story

### The Idea That Changed Computing

In 1953, Hans Peter Luhn at IBM filed an internal memorandum describing 
a technique for storing and retrieving records by their content rather 
than their location...
```

This isn't fluff — it's context. A reader who understands the history understands the tradeoffs.

### 2. Architecture / How It Works

Before API details, explain the mechanism. Use diagrams.

**Pattern:**
```markdown
## Understanding Hash Map Architectures

### Memory Layout Comparison

[Mermaid diagram showing std::unordered_map vs StableHashMap memory layout]

### The Cache Line Effect

Modern CPUs don't fetch individual bytes from memory; they fetch 
*cache lines* — typically 64 bytes at a time...
```

The reader should understand *why* the component is fast before learning *how* to use it.

### 3. Getting Started (Quick Start)

Now — and only now — show basic usage:

```markdown
## Getting Started

### Prerequisites and Integration

StableHashMap requires C++17 and has no dependencies...

### Your First StableHashMap

```cpp
#include "StableHashMap.h"
// Complete, compilable example
```
```

### 4. Deep-Dive Feature Sections (Recipes)

Each major feature gets its own section with:
1. **The problem it solves** (Why does this exist?)
2. **How it works** (Mechanism, not just syntax)
3. **Complete examples** (Copy-paste ready)
4. **Gotchas** (What goes wrong)

**Pattern:**
```markdown
## The Insert Dilemma: Four Methods, Four Philosophies

### Why So Many Insert Methods?

StableHashMap provides four ways to add elements: `insert()`, 
`insert_or_assign()`, `emplace()`, and `try_emplace()`. This seems 
redundant. Why not just one?

The answer involves a fundamental design question: **what happens 
when you insert a key that already exists?**

Different use cases want different answers:
- Configuration: "Update the setting if it exists..." → overwrite
- Caching: "Use the cached value if present..." → don't overwrite
```

### 5. Error Handling Model

Explicit documentation of:
- What throws vs returns Expected vs terminates
- Debug vs Release behavior
- How to configure error policies

### 6. Performance Notes

What matters, what doesn't. Rules of thumb:
```markdown
## Performance Rules of Thumb

- **Reserve before bulk insert:** Avoids N reallocations
- **Use try_emplace for expensive values:** Avoids construction if key exists
- **Prefer references over copies:** `auto& val = map[key]` not `auto val = map[key]`
```

### 7. When to Use / When Not To

Explicit guidance. Don't make the reader guess.

```markdown
## When to Use StableHashMap (and When Not To)

### Use StableHashMap When:
- Large datasets (>1000 elements)
- Erase-heavy workloads
- Need pointer stability across mutations

### Don't Use StableHashMap When:
- Types aren't DefaultConstructible
- Memory is severely constrained
- You need SIMD-accelerated lookups (use FastHashMap)
```

### 8. Migration Guide

Side-by-side comparison with std:: equivalent. Call out every semantic difference.

```markdown
## Migration from std::unordered_map

### Critical Differences

**1. emplace() overwrites existing values**

```cpp
// std::unordered_map: emplace ignores duplicate
std_map.emplace(1, "first");
std_map.emplace(1, "second");  // IGNORED

// StableHashMap: emplace OVERWRITES
stable_map.emplace(1, "first");
stable_map.emplace(1, "second");  // OVERWRITES!
```
```

### 9. Troubleshooting

Organized by symptom. Include compilation errors, runtime errors, and performance issues.

```markdown
## Troubleshooting

### Compilation Errors

**"StableHashMap requires DefaultConstructible Key and Value"**

Your key or value type lacks a default constructor...

### Runtime Errors

**Assertion failure: "mutation attempted in read-only mode"**

You called insert() on a frozen map...

### Performance Issues

**Operations slow at high load factor**

Insert/find/erase cost grows exponentially above 0.80 load...
```

### 10. API Reference (Concise)

At the **end**, not the beginning. By now the reader understands the concepts; this is for quick lookup.

```markdown
## API Reference

### Construction

| Signature | Description |
|-----------|-------------|
| `StableHashMap()` | Default constructor, empty map |
| `StableHashMap(size_t n)` | Pre-allocate for n elements |

### Insertion

| Method | Overwrites? | Returns |
|--------|------------|---------|
| `insert(k, v)` | No | `bool` |
| `insertOrAssign(k, v)` | Yes | `pair<Value*, bool>` |
```

### 11. FAQ

Short answers to common questions.

### 12. Appendices (Optional)

For advanced topics that don't fit the main flow.

## Writing Style for User Manuals

### Teach, Don't Just Document

**Wrong:**
```markdown
### insert()

```cpp
bool insert(const Key& k, const Value& v);
```

Inserts a key-value pair. Returns true if inserted.
```

**Right:**
```markdown
### insert(): Insert-Only (No Overwrite)

`insert()` only inserts if the key is **missing**. If the key exists, 
it does nothing and returns `false`:

```cpp
fat_p::StableHashMap<std::string, int> map;
map.insert("x", 1);
bool inserted = map.insert("x", 2);  // Returns false, value unchanged
std::cout << *map.find("x");  // Prints 1 (original value)
```

**This matches `std::unordered_map::insert()` semantics** — duplicates 
are ignored. Use `insertOrAssign()` for upsert behavior.
```

### Use Section Titles That Ask Questions

**Wrong:** "Insert Methods"
**Right:** "The Insert Dilemma: Four Methods, Four Philosophies"

### Explain the Why Before the How

**Wrong:**
```markdown
## Read-Only Mode

Call `freeze()` to enable read-only mode.
```

**Right:**
```markdown
## Read-Only Mode: Trading Flexibility for Density

### Why Freeze?

A mutable hash table must maintain slack for future insertions...
```

### Be Honest About Limitations

**Wrong:** (silence about when the component is worse)

**Right:**
```markdown
### Where StableHashMap Loses

For miss-heavy lookups where most queries return "not found," 
SIMD-accelerated tables like SwissTable can be faster...
```

## User Manual Template

```markdown
---
doc_id: UM-0000
doc_type: "User Manual"
title: "[Component]"
fatp_components: ["[Component]"]
topics: ["..."]
cxx_standard: "C++17"
last_verified: "YYYY-MM-DD"
audience: ["C++ developers", "AI assistants"]
status: "draft"
---

# User Manual - [Component]

**Scope:** ...
**Not covered:** ...
**Prerequisites:** ...

## [Opening Story / The Hook]
...

## Architecture / How It Works
...

## Quick Start
...

## Recipes
- Recipe A
- Recipe B

## Error Handling Model
...

## Performance Rules of Thumb
...

## When to Use / When Not To
...

## Migration Guide
...

## Troubleshooting

### Compilation Errors
...

### Runtime Errors
...

### Performance Issues
...

## API Reference
...

## FAQ
...
```

## User Manual Checklist

### Structure
- [ ] YAML header present
- [ ] Opens with story/context, not API
- [ ] Architecture explained before usage
- [ ] Each feature section explains WHY before HOW
- [ ] Error handling model documented
- [ ] Performance rules of thumb included
- [ ] "When to use / When not to use" section
- [ ] Migration guide with side-by-side code
- [ ] Troubleshooting organized by symptom
- [ ] API reference at the end, not the beginning

### Content
- [ ] Every feature explains the problem it solves
- [ ] Every code example is complete and compilable
- [ ] Every semantic difference from std:: is called out
- [ ] Every common mistake is documented with fix
- [ ] Diagrams where memory layout or flow matters

### Tone
- [ ] Teaching, not just documenting
- [ ] Section titles that engage ("The Insert Dilemma")
- [ ] Honest about where the component loses
- [ ] Explains concepts an intelligent non-expert might not know

## Common Failure Mode

Manuals that don't warn about build-mode behavior or error model. A manual must be explicit about failure semantics.

---

# 3. COMPANION GUIDE

## Intent

Explain **why** the design exists:
- rationale
- tradeoffs
- rejected alternatives
- philosophy
- how multiple components work together

A Companion Guide differs from an Overview: Overviews showcase individual components; Companion Guides tell the story of an entire **problem domain** and how multiple components address it.

## Relationship to User Manual

Companion Guides and User Manuals may cover similar ground. This is intentional:

| Emphasis | User Manual | Companion Guide |
|----------|-------------|-----------------|
| Primary question | "How do I use this?" | "Why does this exist?" |
| Problem coverage | Briefly, to motivate | Deeply, with failure modes |
| Code examples | Practical patterns | Illustrating concepts |
| Case studies | Brief "common patterns" | Extended real-world scenarios |
| Audience | Users who've decided to use it | Engineers evaluating or seeking deep understanding |

## The Four-Part Arc

Every Companion Guide MUST follow this narrative structure:

### Part I — The Problems
Show the reader what goes wrong and WHY. Ground every problem in real constraints.

**Wrong:** "Serialization can be error-prone."
**Right:** "You serialize a struct with a uint32_t field. Six months later, someone changes it to uint64_t. The deserializer reads 4 bytes, interprets garbage as valid data, and your system silently corrupts a production database."

### Part II — The Solutions
Map each problem to a specific component. Explain the mechanism, not just the API.

**Wrong:** "Use BinarySerializer for type-safe serialization."
**Right:** "BinarySerializer embeds type tags and version numbers in the stream header. On deserialization, it compares the stream's type signature against the expected type at compile time. Mismatch? You get a compile error, not runtime corruption."

### Part III — The Case Studies
Show complete stories with symptoms, investigations, fixes, and measured results.

**Wrong:** "Here's how to use DiagnosticLogger."
**Right:** "The trading system logged 50,000 messages per second. Latency spikes of 200μs occurred every few seconds. The culprit: string formatting in the hot path. We switched to structured logging with deferred formatting. Median latency dropped from 12μs to 2μs; P99 from 200μs to 8μs."

### Part IV — The Foundations
Provide design rationale and deep technical explanations.

**Wrong:** "JsonLite is a simple JSON parser."
**Right:** "JsonLite exists because nlohmann/json prioritizes ergonomics over allocation control. In HPC contexts, you need to parse JSON into pre-allocated buffers, reuse parse state across calls, and never touch the heap in the hot path. JsonLite's pull-parser architecture enables all three."

## The Three Pillars

Every component description MUST demonstrate:

### 1. Constraint Grounding
Explain which real-world constraint the component addresses:

| Domain | Typical Constraints |
|--------|---------------------|
| HPC | Cache lines, NUMA topology, SIMD lanes, memory bandwidth |
| Serialization | Wire format stability, version skew, zero-copy parsing |
| Logging | Hot-path overhead, structured output, sink flexibility |
| Data Structures | Allocation frequency, iterator stability, cache locality |
| Concurrency | Lock contention, memory ordering, wait-free progress |
| Error Handling | Exception-free codepaths, error context preservation |

### 2. Mechanism Visibility
Show HOW the component achieves its guarantees, not just WHAT it does.

### 3. Guarantee Explicitness
State what the component guarantees and what it does NOT guarantee.

## Required Sections

### Title and Scope

**Formula:**
1. Evocative title capturing the problem domain
2. Subtitle identifying the library and scope
3. Explicit scope statement

**Example:**
> # The Data Boundary
> ### A Companion Guide to FAT-P's Serialization Components
> 
> **Scope:** This guide covers FAT-P's serialization utilities. Network protocols, compression, and encryption are not covered.

### Problem Chapters (Part I)

Each chapter must include:
1. **The Obvious Approach** — What most developers do
2. **The Hidden Constraint** — Why it fails
3. **The Symptoms** — How this manifests
4. **The Cost** — Specific impact
5. **The Solution Preview** — Brief
6. **Forward Reference** — "Part IV explains..."

### Solution Chapters (Part II)

Each chapter must include:
1. **Problem link** — "Part I showed..."
2. **The Mechanism** — HOW it works
3. **Guarantees / Non-Guarantees table**
4. **Decision guide** — When to choose each option
5. **Where it loses** — Honest boundaries

### Case Study Chapters (Part III)

Each must include:
1. **Context** — What the system does
2. **Initial approach** — Problematic code/design
3. **Observing the symptoms** — Specific metrics
4. **The Fix** — Corrected approach
5. **Results** — Before/after with numbers
6. **Components Used** — Explicit list with roles
7. **Transferable Lessons** — Patterns that apply beyond this case

### Foundation Appendices (Part IV)

- **Design rationale** — Why the API is shaped this way
- **Rejected alternatives** — What was considered
- **Edge cases** — How they're handled
- **When to Look Elsewhere** — External alternatives

## Power Phrases by Domain

**Serialization:**
- "Wire-format stability across versions"
- "Zero-copy deserialization"
- "Type-safe at compile time, efficient at runtime"

**Logging:**
- "Structured over stringly-typed"
- "Deferred formatting"
- "Zero-cost when disabled"

**Data Structures:**
- "Pointer stability under mutation"
- "Cache-friendly iteration"
- "Allocation-free operation"

**Concurrency:**
- "Lock-free progress guarantee"
- "Wait-free for readers"
- "ABA-safe"

## Metrics Standards

### Always Quantify

**Wrong:** "Much faster"
**Right:** "3.2x faster (156ms → 49ms)"

**Wrong:** "More reliable"
**Right:** "Zero serialization failures in 6 months of production (previously: 2-3 per week)"

### Domain-Appropriate Metrics

| Domain | Key Metrics |
|--------|-------------|
| HPC | IPC, cache miss rate, memory bandwidth, FLOPS |
| Serialization | Bytes/sec, allocation count, round-trip correctness |
| Logging | Messages/sec, P50/P99 latency, memory per message |
| Data Structures | Ops/sec, memory overhead ratio, cache misses per op |
| Concurrency | Contention rate, wait time, throughput under load |

### Always Acknowledge Variance

**Wrong:** "Runs in 2.5ms"
**Right:** "Median 2.5ms, P99 4.1ms, max observed 12ms"

## Code Example Standards

### Good Example (shows the trap and the fix):
```cpp
// THE TRAP: String formatting in hot path
for (const auto& order : orders) {
    logger.info("Processing order " + std::to_string(order.id) + 
                " for $" + std::to_string(order.amount));  // 3 allocations per log
    process(order);
}

// THE FIX: Structured logging with deferred formatting
for (const auto& order : orders) {
    LOG_INFO("order.process", 
             Field("order_id", order.id), 
             Field("amount", order.amount));  // Zero allocations; formatted at sink
    process(order);
}
```

### Required Code Comments
- `// THE TRAP:` before problematic code
- `// THE FIX:` before corrected code
- `// N allocations` to highlight allocation behavior
- `// O(1)` or `// O(n)` for complexity-critical paths
- `// Thread-safe` or `// Not thread-safe` where relevant

## Companion Guide Template

```markdown
---
doc_id: CG-0000
doc_type: "Companion Guide"
title: "[Domain Name]"
fatp_components: ["...", "..."]
topics: ["..."]
constraints: ["..."]
cxx_standard: "C++17"
last_verified: "YYYY-MM-DD"
audience: ["C++ developers", "AI assistants"]
status: "draft"
---

# Companion Guide - [Evocative Domain Title]

**Scope:** ...
**Not covered:** ...
**Prerequisites:** ...

## The problem we're solving
...

## Part I — The Problems

### Chapter 1: [Problem Name]

**The obvious approach:** ...
**The hidden constraint:** ...

```cpp
// THE TRAP: [Description]
[Problematic code]
```

**The symptoms:** ...
**The cost:** ...
**What FAT-P provides:** ...

---

## Part II — The Solutions

### Chapter N: [Component Name]

Chapter M described [problem]. [Component] addresses this by [mechanism].

| Guarantee | Provided | Notes |
|-----------|----------|-------|
| ... | Yes/No | ... |

**Where it loses:** ...

---

## Part III — Case Studies

### Case Study: [Context]

**Context:** ...
**Initial approach:** ...
**Symptoms:** ...
**Fix:** ...
**Results:**

| Metric | Before | After | Improvement |
|--------|--------|-------|-------------|
| ... | ... | ... | ... |

**Components Used:**
- `ComponentA` — [Role]

**Transferable Lessons:** ...

---

## Part IV — Foundations

### Design Rationale
...

### Rejected Alternatives
...

### When to Look Elsewhere
...

---

## Read Next
- [User Manual - ...]
- [Case Study - ...]
```

## Companion Guide Checklist

### Structure
- [ ] Four-part arc (Problems → Solutions → Case Studies → Foundations)
- [ ] Table of Contents with working anchor links
- [ ] Scope statement explicitly lists what's covered and what isn't
- [ ] Each Part I chapter links forward to Part IV
- [ ] Each Part II chapter references the Part I problem it solves
- [ ] "Components Used" section in every case study

### Technical Content
- [ ] Every claim explains the mechanism, not just the benefit
- [ ] Every component has a Guarantees table
- [ ] Quantified metrics (not "faster" but "3.2x")
- [ ] "Where it loses" section included
- [ ] Integration points with other FAT-P components documented

### Code
- [ ] All examples show THE TRAP and THE FIX pattern
- [ ] No syntax-only examples
- [ ] Allocation counts, complexity, thread-safety annotated

## Common Failure Mode

Becoming a reference manual. Companion Guides explain "why," not every API detail.

---

# 4. CASE STUDY

## Intent

Teach via one focused failure mode:
- show the trap
- prove the mechanism
- show the fix
- generalize the lesson

A Case Study is a **focused engineering lesson** with proof. It exists to teach:
1. A **real constraint** (hardware, allocator, optimizer, UB rules, ABI, concurrency)
2. The **mechanism** that makes the obvious approach fail
3. A **repeatable fix pattern**
4. How FAT-P's design embodies the fix

## Audience

- **Primary human readers:** competent C++ developers who want sharper intuition about UB, allocators, CPU caches, exception boundaries
- **Primary AI readers:** retrieval + reasoning systems that need unambiguous structure, explicit guarantees, and citations

## The Four-Part Arc (Required)

### Part I — The Problems
- The **obvious** approach
- The **hidden constraint**
- The **symptoms** (specific, not vague)
- The **cost** (numbers when possible)

**Required substructure:**
1. **The Obvious Approach**
2. **The Hidden Constraint**
3. **The Symptoms**
4. **The Cost**
5. **The Solution Preview** (1-3 sentences)
6. A forward reference: "Part IV explains..."

### Part II — The Solutions
- Fix patterns as **mechanisms**, not slogans
- Tradeoffs and when **not** to use the fix
- Where FAT-P fits (components/policies)

**Required substructure:**
1. **Problem link** ("Part I showed...")
2. **The Mechanism**
3. **Guarantees / Non-Guarantees table**
4. **Decision guide** (when to choose each option)
5. **Where it loses** (the honest boundary)

### Part III — The Case Study Story
The detective narrative with evidence.

**Required substructure:**
1. **Context**
2. **Initial approach**
3. **Observations**
4. **Hypotheses**
5. **Evidence**
6. **Fix**
7. **Results**
8. **Components Used**
9. **Transferable Lessons**

### Part IV — Foundations
Deep rationale and audit-ready documentation.

**Required substructure:**
- **Design rationale**
- **Rejected alternatives**
- **Edge cases**
- **Guarantee table(s)**
- **Mechanical checklist**

### Required Ending
1. **Design Rules to Internalize** (3-7 bullets)
2. **What To Do Now** (action list + pitfalls)

## The Four Pillars

Every Case Study MUST demonstrate:

### 1. Constraint Grounding
Name the constraint that shaped the failure: cache lines, allocator metadata, branch predictors, exception boundaries, UB rules, etc.

### 2. Mechanism Visibility
Show what really happens: call path, memory layout, optimizer reasoning, destruction timing, etc.

### 3. Guarantee Explicitness
State what is guaranteed and what is not. Never rely on reader inference.

### 4. AI Legibility
Write so an AI can safely answer questions from the document: stable headings, explicit definitions, evidence quotes, no hand-wavy claims, clear separation of facts vs hypotheses.

## Required Front Matter

1. **Title** (problem-focused, evocative)
2. **Subtitle** (what concept + which FAT-P specimen)
3. **Scope** (what is covered)
4. **Not covered** (explicit exclusions)
5. **Prerequisites** (what you assume the reader knows)
6. **Table of Contents** (required once > ~10 headings or > ~800 lines)

## Required Opening: The Trap

### "⚠️ Before You Read Further: [The Trap]"

Requirements:
- A **tiny snippet** that looks reasonable
- A blunt callout: "Stop." / "This is catastrophic."
- A concrete consequence: allocations per iteration, UB exposure, termination behavior, silent corruption
- A "what you think vs what happens" framing

**Example:**
```markdown
## ⚠️ Before You Read Further: The Silent Pointer Chase

```cpp
// THE TRAP: Process all tag matches, then check for empty
match_mask = group.match(h2);
for each bit in match_mask { node_deref(); key_equal(); }  // ← 0.12 extra derefs/miss
if (empty_mask) return npos;
```

Stop. At N=1,000,000 this costs 7 nanoseconds per miss—invisible at small N, dominant at scale.
```

### If the opening is hypothetical

If your opening relies on "if X returns" or "if the compiler assumes Y," add:

> "This is a model of what happens if the primitive can return. In Part III we audit what FAT-P's primitive actually does."

## Case Study Card (Required)

Near the top, include:

```markdown
## Case Study Card

**Problem:** Miss latency 3× slower than competitor
**Constraint:** Reference stability requires node indirection
**Symptom:** Eq/miss = 0.12 (expected ≈ 0.007)
**Root cause:** Processing tag matches after first empty
**Fix pattern:** Mask match_mask with (empty & -empty) - 1
**FAT-P components used:** StableHashMap, FatPBenchmarkRunner, MissDiag
**Build-mode gotchas:** None (logic bug, not UB)
**Guarantees:** Miss terminates at first empty
**Non-guarantees:** Not O(1) worst-case under adversarial H2
```

## Evidence Standards

### Rule: Never invent evidence

If the doc contains numbers, they must be backed by:
- benchmark output
- profiler screenshots
- sanitizer output
- or a clearly defined theoretical count ("3 allocations per iteration because...")

If something is not proven, label it as **Hypothesis** and do not base the fix solely on it.

### Performance Case Studies MUST include:
- CPU (or class: x86-64 desktop/server, AArch64)
- Compiler + version
- Flags (e.g., `/O2`, `-O3`, LTO, sanitizers)
- Run counts, variance (median/P99 or mean±stddev)
- Before/after table with units

### Correctness/UB Case Studies MUST include at least one:
- Sanitizer output (UBSan/ASan/TSan)
- Compiler warning explanation
- Minimal reproduction program
- Or a concrete audit trace

### Audit Trace Format (AI-friendly)

When auditing behavior, use this explicit chain:

> macro → policy tag → raiser selection → when failure triggers (immediate vs RAII destructor) → what failure does (throw/abort/return) → what `noexcept` implies

Include file and symbol names.

## Guarantee / Non-Guarantee Tables (Required)

Every Case Study MUST include at least one table like:

```markdown
| Property | Guaranteed? | Conditions | Notes |
|----------|-------------|------------|-------|
| Miss terminates at first empty | ✅ Yes | All cases | Bitmask truncation |
| Tag/miss ≈ LF/(128*(1-LF)) | ✅ Yes | Random keys | Probabilistic bound |
| O(1) worst-case | ❌ No | Adversarial H2 | Collisions possible |
```

## AI Readability Rules (Critical)

These rules are required because AI is a primary client:

### 1. Stable headings
Use consistent headings across documents:
- "Scope"
- "Not covered"
- "The Hidden Constraint"
- "Guarantees / Non-Guarantees"
- "Where it loses"
- "What To Do Now"
- "Mechanical Audit Checklist"
- "Glossary"

### 2. Define terms once, then reuse the same label
If you define "control-flow barrier," keep using that phrase, not five synonyms.

### 3. Avoid ambiguous pronouns
Prefer "this destructor," "this macro expansion," "this overflow path" instead of "this" / "it" without a noun.

### 4. Separate facts from hypotheses
Use explicit labels:
- **Fact:** backed by evidence/audit/measurement
- **Hypothesis:** plausible but not proven

## Case Study Template

```markdown
---
doc_id: CS-0000
doc_type: "Case Study"
title: "[Evocative Problem Name]"
fatp_components: ["..."]
topics: ["..."]
constraints: ["..."]
cxx_standard: "C++17"
build_modes: ["Debug", "Release"]
last_verified: "YYYY-MM-DD"
audience: ["C++ developers", "AI assistants"]
status: "draft"
---

# Case Study - [Evocative Problem Name]
## [Subtitle: concept + FAT-P specimen]

**Scope:** ...
**Not covered:** ...
**Prerequisites:** ...

## Case Study Card

**Problem:** ...
**Constraint:** ...
**Symptom:** ...
**Root cause:** ...
**Fix pattern:** ...
**FAT-P components used:** ...
**Build-mode gotchas:** ...
**Guarantees:** ...
**Non-guarantees:** ...

## Table of Contents
...

## ⚠️ Before You Read Further: [The Trap]

```cpp
// THE TRAP:
...
```

[Blunt consequence statement]

---

## Part I — The Problems

### The Obvious Approach
...

### The Hidden Constraint
...

### The Symptoms
...

### The Cost
...

### Solution Preview
...

*Part IV explains [forward reference].*

---

## Part II — The Solutions

### The Mechanism
...

### Guarantees / Non-Guarantees

| Property | Guaranteed? | Conditions | Notes |
|----------|-------------|------------|-------|
| ... | ... | ... | ... |

### Decision Guide
...

### Where It Loses
...

---

## Part III — The Case Study Story

### Context
...

### Initial Approach
```cpp
[Original code]
```

### Observations
...

### Hypotheses
...

### Evidence
[Benchmark/sanitizer/profiler output]

### The Fix
```cpp
// THE FIX:
[Corrected code]
```

### Results

| Metric | Before | After | Change |
|--------|--------|-------|--------|
| ... | ... | ... | ... |

### Components Used
- `ComponentA` — [Role]

### Transferable Lessons
...

---

## Part IV — Foundations

### Design Rationale
...

### Rejected Alternatives
...

### Edge Cases
...

### Mechanical Audit Checklist
- [ ] ...

---

## Design Rules to Internalize
- Rule 1
- Rule 2
- Rule 3

## What To Do Now
1. Action 1
2. Action 2
3. Watch out for: [pitfall]
```

## Case Study Checklist

### Structure
- [ ] YAML header present
- [ ] H1 matches filename prefix
- [ ] Scope + Not covered + prerequisites
- [ ] Case Study Card present
- [ ] "⚠️ Before You Read Further" trap with quantified consequence
- [ ] Four-Part Arc present with all required substructure
- [ ] Guarantee / Non-Guarantee table present
- [ ] Design Rules + What To Do Now present

### Technical Accuracy
- [ ] Every non-obvious claim backed by evidence or labeled hypothesis
- [ ] Debug vs release behavior called out where relevant
- [ ] Verbatim/excerpt/pseudocode labeling consistent
- [ ] No invented benchmarks, test outcomes, or compilation claims

### AI Legibility
- [ ] Stable headings used
- [ ] Terms defined once and reused
- [ ] No ambiguous "this/it" without referent in critical sections

## Common Failure Mode

Multiple unrelated traps. If you have multiple failure modes, split into multiple case studies.

---

# 5. FOUNDATIONS

## Intent

Teach background: "what you need to know to reason about the rest."

Foundations documents provide the **conceptual grounding** that makes other documents make sense. They answer questions like:
- Why does C++ have this semantics?
- What does the hardware actually do?
- What mental models should I use?
- What myths are commonly believed but wrong?

Examples:
- History of C++ exception semantics decisions
- UB mental models and optimizer assumptions
- Memory model primer for concurrency
- Floating point quirks and IEEE 754
- Cache hierarchy and memory latency
- Allocator design space

## Relationship to Other Docs

Foundations documents are **prerequisite reading** for other doc types:
- Case Studies assume you understand the constraint being exploited
- Companion Guides assume you understand the domain
- User Manuals may link to Foundations for "deep background"

A Foundations doc teaches **concepts**, not "how to call functions."

## Required Sections

### 1. Scope and Prerequisites

```markdown
**Scope:** This document explains C++'s undefined behavior semantics and what compilers are allowed to assume.

**Not covered:** Specific FAT-P components (see Case Studies), language lawyer edge cases, implementation-defined behavior.

**Prerequisites:** Basic C++ syntax, understanding of compilation vs runtime.
```

### 2. Key Concepts

The core ideas, explained from first principles. Use progressive disclosure: start with the intuition, then add precision.

**Pattern:**
```markdown
## Key Concepts

### What is Undefined Behavior?

Most programmers think UB means "the program might crash." This is incomplete.

UB means the compiler is allowed to assume the condition **never happens**. This assumption propagates backward in time — the compiler can optimize code that *leads to* UB, not just code that *executes after* UB.

**Concrete example:**
```cpp
int* p = get_pointer();
if (p == nullptr) {
    log("null pointer");
}
*p = 42;  // UB if p is null
```

A sufficiently clever compiler can observe that `*p` would be UB if p is null. Therefore, p is never null. Therefore, the null check is dead code. Therefore, the log call is removed.

This is not a bug. This is optimization.
```

### 3. Myths vs Reality

Explicitly address common misconceptions:

```markdown
## Myths vs Reality

### Myth: "UB only matters if the code actually executes"

**Reality:** UB allows the compiler to reason about paths that *lead to* the UB, not just the UB itself. An unreachable `*nullptr` can still affect reachable code.

### Myth: "noexcept means the function doesn't throw"

**Reality:** `noexcept` means "if this function throws, call std::terminate()." The function can still throw — you just get termination instead of propagation.
```

### 4. Why It Matters for Systems Code

Connect the abstract concepts to concrete engineering concerns:

```markdown
## Why It Matters for Systems Code

### Memory Models and Data Races

In single-threaded code, you can reason about memory as a simple array of bytes. In multi-threaded code, this model breaks down:

- Reads and writes can be reordered by the compiler
- Reads and writes can be reordered by the CPU
- Without synchronization, threads may see stale values indefinitely

FAT-P's concurrency components (LockFreeQueue, AtomicSharedPtr) encode these constraints in their type system...
```

### 5. Implications for FAT-P (Brief)

A short section connecting to specific library components:

```markdown
## Implications for FAT-P

- `enforce` uses noexcept-aware policy selection to avoid terminate in Release builds
- `Expected` provides a return-based error model that works in noexcept contexts
- `CheckedArithmetic` makes overflow defined behavior through explicit policy
```

### 6. Glossary

Define all terms introduced:

```markdown
## Glossary

**Undefined Behavior (UB):** Program state that violates language rules, allowing arbitrary compiler behavior.

**Sequence Point:** A point in execution where all prior side effects are complete.

**Memory Order:** Constraints on how memory operations can be reordered across threads.
```

### 7. Further Reading (Optional but encouraged)

```markdown
## Further Reading

- [What Every Programmer Should Know About Memory](https://people.freebsd.org/~lstewart/articles/cpumemory.pdf) — Ulrich Drepper
- [C++ Memory Model](https://en.cppreference.com/w/cpp/language/memory_model) — cppreference
- ISO C++ Standard §6.9.1 [intro.execution] — Undefined behavior
```

## Foundations Template

```markdown
---
doc_id: FN-0000
doc_type: "Foundations"
title: "[Concept Name]"
fatp_components: []
topics: ["..."]
constraints: ["..."]
cxx_standard: "C++17"
last_verified: "YYYY-MM-DD"
audience: ["C++ developers", "AI assistants"]
status: "draft"
---

# Foundations - [Concept Name]

**Scope:** ...
**Not covered:** ...
**Prerequisites:** ...

## Key Concepts

### [Concept 1]
[Intuition first, then precision]

```cpp
// Example illustrating the concept
```

### [Concept 2]
...

---

## Myths vs Reality

### Myth: "[Common misconception]"
**Reality:** [Correct understanding]

### Myth: "[Another misconception]"
**Reality:** [Correct understanding]

---

## Why It Matters for Systems Code

### [Specific Concern 1]
...

### [Specific Concern 2]
...

---

## Implications for FAT-P

- `Component1` addresses [concept] by [mechanism]
- `Component2` provides [guarantee] through [approach]

---

## Glossary

**Term 1:** Definition.
**Term 2:** Definition.

---

## Further Reading

- [Resource 1]
- [Resource 2]
```

## Foundations Checklist

- [ ] YAML header present
- [ ] Scope + Not covered + prerequisites
- [ ] Key concepts explained from first principles
- [ ] Myths vs Reality section addresses common misconceptions
- [ ] Connection to systems engineering concerns
- [ ] Brief tie-in to FAT-P components
- [ ] Glossary defines all introduced terms
- [ ] Further reading provided (optional)
- [ ] No API details (that's the Manual)
- [ ] No extended case studies (that's Case Study)

## Common Failure Mode

Turning into a Manual. Foundations teach concepts, not "how to call functions."

---

# 6. HANDBOOK

## Intent

Codify discipline: how teams should think and act.

A Handbook is a **methodology document** that establishes:
- Principles to guide decisions
- Hard rules vs. rules of thumb
- Checklists for common situations
- Anti-patterns to avoid
- Adoption guidance for teams

Examples:
- Performance Invariants Handbook
- Class Design Discipline Handbook
- Benchmarking Methodology Handbook
- Error Handling Strategy Handbook
- Thread Safety Audit Handbook

## Relationship to Other Docs

| Doc Type | Focus |
|----------|-------|
| User Manual | How to use a component |
| Case Study | Why a specific thing failed |
| **Handbook** | What discipline to apply across all work |

A Handbook is about **habits and standards**, not specific components.

## Required Sections

### 1. Principles

The high-level philosophy that guides everything else:

```markdown
## Principles

### 1. Measure Before Claiming

No performance claim without data. "It's faster" is not acceptable; "3.2x faster at N=10K, median of 50 runs" is.

### 2. Counters Before Time

Time is a symptom. Measure the event upstream of time: allocations, cache misses, branch mispredicts. When the counter moves, time follows.

### 3. Adversarial Thinking

If your benchmark shows improvement, try to break it. What input pattern defeats your optimization?
```

### 2. Hard Rules vs. Rules of Thumb

Distinguish between non-negotiable requirements and situational guidance:

```markdown
## Hard Rules

These are non-negotiable. Violation is a defect.

1. **All benchmark results must include platform specification** (OS, compiler, CPU, flags)
2. **All claims must cite methodology** (run count, statistic, variance)
3. **All comparisons must use identical input data**

## Rules of Thumb

These are strong guidance. Deviation requires justification.

1. **Prefer median over mean** — medians resist outliers
2. **Use at least 50 runs for timing** — less may be insufficient for stable estimates
3. **Test at least 3 sizes** — small N hides memory effects
```

### 3. Checklists

Actionable lists for specific situations:

```markdown
## Checklists

### Before Publishing a Benchmark Result

- [ ] Platform documented (OS, compiler, CPU)
- [ ] Compiler flags documented
- [ ] Run count stated
- [ ] Statistic stated (median, mean, P99)
- [ ] Variance/range stated
- [ ] Comparison baseline identified
- [ ] Caveats section present

### Before Claiming an Optimization

- [ ] Before/after numbers with identical methodology
- [ ] Multiple sizes tested
- [ ] Adversarial input tested
- [ ] Counter movement explains time movement
- [ ] No regression in other operations
```

### 4. Anti-Patterns

Explicit examples of what NOT to do:

```markdown
## Anti-Patterns

### The Single-Run "Proof"

**What happens:** Developer runs benchmark once, sees 20% improvement, claims victory.

**Why it's wrong:** Single runs have high variance. The "improvement" may be noise, thermal throttling, or background processes.

**What to do instead:** Run at least 50 iterations. Report median and variance. If variance is high, investigate.

### The Micro-Benchmark Extrapolation

**What happens:** Developer shows 10x speedup on a 100-element test, extrapolates to production scale.

**Why it's wrong:** Cache effects dominate at small N. At N=1M, memory bandwidth may be the bottleneck.

**What to do instead:** Test at realistic sizes. Include at least one size that exceeds L3 cache.
```

### 5. Worked Examples

Short case studies demonstrating the discipline in action:

```markdown
## Worked Examples

### Example: Validating a Hash Table Optimization

**Claim:** "New probing strategy reduces miss latency by 40%."

**Discipline applied:**

1. **Measure the counter:** Before change, Eq/miss = 0.12. After change, Eq/miss = 0.01.
2. **Calculate expected improvement:** 0.11 fewer comparisons × ~60ns per comparison = ~6.6ns saved. Observed: 7ns saved. Matches.
3. **Adversarial test:** Generate H2-biased miss set. Performance still improved.
4. **Multi-size test:** Improvement holds at N=10K, N=100K, N=1M.

**Conclusion:** Optimization is valid.
```

### 6. Team Adoption Guidance

How to roll out the discipline:

```markdown
## Adoption Plan

### Phase 1: Awareness (Week 1)

- Share this handbook
- Review one existing benchmark against checklist
- Identify gaps

### Phase 2: Enforcement (Weeks 2-4)

- Add checklist to PR template
- Require peer review of benchmark claims
- Flag violations in code review

### Phase 3: Automation (Month 2+)

- Add CI checks for benchmark format
- Create templates that enforce structure
- Build regression detection
```

## Handbook Template

```markdown
---
doc_id: HB-0000
doc_type: "Handbook"
title: "[Discipline Name]"
fatp_components: []
topics: ["..."]
constraints: ["..."]
cxx_standard: "C++17"
last_verified: "YYYY-MM-DD"
audience: ["C++ developers", "AI assistants"]
status: "draft"
---

# Handbook - [Discipline Name]

**Scope:** ...
**Not covered:** ...
**Prerequisites:** ...

## Principles

### 1. [Principle Name]
...

### 2. [Principle Name]
...

---

## Hard Rules

1. ...
2. ...

## Rules of Thumb

1. ...
2. ...

---

## Anti-Patterns

### [Anti-Pattern Name]

**What happens:** ...
**Why it's wrong:** ...
**What to do instead:** ...

---

## Checklists

### [Situation Name]

- [ ] ...
- [ ] ...

---

## Worked Examples

### Example: [Situation]

**Claim:** ...
**Discipline applied:** ...
**Conclusion:** ...

---

## Adoption Plan

### Phase 1: [Phase Name]
...

### Phase 2: [Phase Name]
...
```

## Handbook Checklist

- [ ] YAML header present
- [ ] Principles section establishes philosophy
- [ ] Hard rules vs. rules of thumb distinguished
- [ ] Anti-patterns section with "what happens / why wrong / what instead"
- [ ] At least one checklist
- [ ] At least one worked example
- [ ] Adoption plan for teams
- [ ] No component-specific API details (that's the Manual)

## Common Failure Mode

Becoming an encyclopedia. Handbooks are discipline documents; structure matters more than completeness.

---

# 7. PATTERN GUIDE

## Intent

Teach a reusable pattern as a recipe.

A Pattern Guide documents a **design or implementation pattern** that solves a recurring problem. It's more specific than a Companion Guide (which covers a domain) and more general than a Case Study (which covers one failure).

Examples:
- Pattern Guide - Factory Pattern Done Right
- Pattern Guide - Error Propagation with Expected
- Pattern Guide - RAII for Resource Management
- Pattern Guide - Policy-Based Design
- Pattern Guide - Type-Safe Flags with EnumPlus

## Relationship to Other Docs

| Doc Type | Focus |
|----------|-------|
| Case Study | One specific failure and fix |
| Companion Guide | Domain-wide rationale |
| **Pattern Guide** | Reusable recipe for a recurring problem |

A Pattern Guide is about **patterns**, not specific bugs.

## Required Sections

### 1. Intent

What the pattern achieves:

```markdown
## Intent

The Factory Pattern separates object creation from object use. When applied correctly, it enables:

1. **Configuration-time decisions** — Which concrete type to create is determined at startup, not scattered through code
2. **Testing flexibility** — Tests can substitute mock implementations without modifying production code
3. **Dependency inversion** — High-level code depends on abstractions, not concrete types
```

### 2. Non-Goals

What the pattern does NOT do:

```markdown
## Non-Goals

The Factory Pattern is NOT:

- A general-purpose DI container (use a proper DI framework if you need full injection)
- A singleton manager (factories can create multiple instances)
- A service locator (factories are explicitly passed, not globally accessed)
```

### 3. When to Use / When to Avoid

Explicit decision criteria:

```markdown
## When to Use

Use this pattern when:
- You need to create objects whose concrete type is determined at runtime or configuration time
- You want to decouple creators from concrete types
- You need to substitute test doubles without #ifdef

## When to Avoid

Avoid this pattern when:
- There's only one concrete type (factory adds indirection for no benefit)
- Creation is trivial and unlikely to change (over-engineering)
- Performance is critical and virtual dispatch is unacceptable (consider templates instead)
```

### 4. The Recipe

Step-by-step implementation:

```markdown
## The Recipe

### Step 1: Define the Interface

```cpp
// Abstract base — what callers depend on
class ILogger {
public:
    virtual ~ILogger() = default;
    virtual void log(Level level, std::string_view message) = 0;
};
```

### Step 2: Implement Concrete Types

```cpp
// Concrete implementations — created by factory
class ConsoleLogger : public ILogger {
public:
    void log(Level level, std::string_view message) override {
        std::cout << "[" << level << "] " << message << "\n";
    }
};

class FileLogger : public ILogger { /* ... */ };
```

### Step 3: Create the Factory

```cpp
// Factory interface
class ILoggerFactory {
public:
    virtual ~ILoggerFactory() = default;
    virtual std::unique_ptr<ILogger> create() = 0;
};

// Concrete factory
class ConsoleLoggerFactory : public ILoggerFactory {
public:
    std::unique_ptr<ILogger> create() override {
        return std::make_unique<ConsoleLogger>();
    }
};
```

### Step 4: Inject the Factory

```cpp
class Application {
public:
    explicit Application(ILoggerFactory& factory)
        : mLogger(factory.create()) {}
    
private:
    std::unique_ptr<ILogger> mLogger;
};
```
```

### 5. Variants

Alternative forms of the pattern:

```markdown
## Variants

### Function-Based Factory

When you don't need a factory *object*, a function suffices:

```cpp
using LoggerFactory = std::function<std::unique_ptr<ILogger>()>;

void configure(LoggerFactory factory) {
    auto logger = factory();
}
```

### Template-Based Factory

When the type is known at compile time but you want deferred construction:

```cpp
template<typename T>
class Factory {
public:
    std::unique_ptr<T> create() { return std::make_unique<T>(); }
};
```

### Registry Factory

When concrete types are registered by name:

```cpp
class LoggerRegistry {
    std::unordered_map<std::string, LoggerFactory> mFactories;
public:
    void registerFactory(std::string name, LoggerFactory f);
    std::unique_ptr<ILogger> create(const std::string& name);
};
```
```

### 6. Reference Implementation

Complete, working code (or link to FAT-P component):

```markdown
## Reference Implementation

See `Factory.h` for FAT-P's policy-based factory implementation:

```cpp
#include "Factory.h"

// Define factory with policies
using MyFactory = fat_p::Factory<
    ILogger,
    fat_p::RegistryPolicy,      // Registration by name
    fat_p::ThreadSafePolicy     // Thread-safe registration
>;
```
```

### 7. Pitfalls

What can go wrong:

```markdown
## Pitfalls

### Pitfall: Factory Explosion

**Symptom:** One factory class per concrete type, leading to dozens of trivial classes.

**Fix:** Use function-based factories or templates. Reserve class-based factories for when you need state.

### Pitfall: Hidden Dependencies

**Symptom:** Factory is accessed globally, hiding what components depend on.

**Fix:** Pass factories explicitly. If injection is too verbose, use a builder or configuration object.
```

### 8. FAQ

```markdown
## FAQ

**Q: Should factories be singletons?**

A: Usually no. Singletons create hidden global state. Pass factories explicitly.

**Q: Can I combine factory with builder?**

A: Yes. Factory decides *which* type to create; builder configures *how* to create it.
```

## Pattern Guide Template

```markdown
---
doc_id: PG-0000
doc_type: "Pattern Guide"
title: "[Pattern Name]"
fatp_components: ["..."]
topics: ["..."]
constraints: ["..."]
cxx_standard: "C++17"
last_verified: "YYYY-MM-DD"
audience: ["C++ developers", "AI assistants"]
status: "draft"
---

# Pattern Guide - [Pattern Name]

**Scope:** ...
**Not covered:** ...
**Prerequisites:** ...

## Intent

[What the pattern achieves — 3-5 bullet points]

---

## Non-Goals

[What the pattern is NOT]

---

## When to Use / Avoid

### When to Use
- ...

### When to Avoid
- ...

---

## The Recipe

### Step 1: [Step Name]

```cpp
[Code]
```

### Step 2: [Step Name]

```cpp
[Code]
```

---

## Variants

### [Variant Name]

[Description and code]

---

## Reference Implementation

[Link to FAT-P component or complete code]

---

## Pitfalls

### [Pitfall Name]

**Symptom:** ...
**Fix:** ...

---

## FAQ

**Q:** ...
**A:** ...
```

## Pattern Guide Checklist

- [ ] YAML header present
- [ ] Intent section with clear benefits
- [ ] Non-goals section
- [ ] When to use / when to avoid with explicit criteria
- [ ] Step-by-step recipe with complete code
- [ ] At least one variant
- [ ] Reference implementation (link or inline)
- [ ] Pitfalls section
- [ ] FAQ

## Common Failure Mode

Becoming a case study. If you start with "we hit this specific bug," you may want a case study plus a pattern guide.

---

# 8. DESIGN NOTE

## Intent

Record a decision clearly enough that future maintainers don't re-litigate it.

A Design Note is a **decision record** that captures:
- What decision was made
- What constraints shaped it
- What alternatives were considered
- Why this option was chosen
- What consequences follow

Design Notes are typically precursors to fuller documentation (Companion Guides, Case Studies) or standalone records for narrow decisions.

## When to Write a Design Note

- You're making a non-obvious architectural choice
- Future readers might ask "why didn't you do X?"
- The decision involves tradeoffs that should be documented
- You rejected obvious alternatives for non-obvious reasons

## Required Sections

### 1. Decision Statement

One sentence stating the decision:

```markdown
## Decision

FAT-P's `enforce` macros use `if constexpr` policy dispatch rather than virtual functions for raiser selection.
```

### 2. Context

What situation prompted the decision:

```markdown
## Context

The enforce system needs to call different failure handlers based on configuration:
- Some contexts want exceptions
- Some contexts want abort()
- Some contexts want logging only

This selection must be:
- Compile-time deterministic (for noexcept analysis)
- Zero-overhead when not invoked
- Auditable (you can trace which handler is selected)
```

### 3. Constraints

What requirements limited the options:

```markdown
## Constraints

1. **Zero runtime overhead** — The selection must not add virtual dispatch cost
2. **noexcept compatibility** — The system must work inside noexcept functions
3. **Auditability** — It must be possible to statically determine which handler is used
4. **C++17 compatibility** — No C++20 concepts or later features
```

### 4. Options Considered

What alternatives were evaluated:

```markdown
## Options Considered

### Option A: Virtual dispatch

```cpp
struct Raiser { virtual void raise(const char*) = 0; };
```

**Pros:** Familiar pattern, runtime flexibility
**Cons:** Virtual dispatch overhead, RTTI dependency, not constexpr

### Option B: Function pointers

```cpp
using RaiserFn = void(*)(const char*);
```

**Pros:** No virtual overhead
**Cons:** Not type-safe, hard to audit, runtime selection

### Option C: if constexpr with policy tags (Selected)

```cpp
template<typename Policy>
void raise(const char* msg) {
    if constexpr (std::is_same_v<Policy, ThrowPolicy>) {
        throw std::logic_error(msg);
    } else if constexpr (std::is_same_v<Policy, AbortPolicy>) {
        std::abort();
    }
}
```

**Pros:** Zero overhead, compile-time auditable, noexcept-friendly
**Cons:** All policies must be known at compile time
```

### 5. Decision

What was chosen and why:

```markdown
## Decision Rationale

Option C (if constexpr with policy tags) was selected because:

1. **Zero overhead** — The unused branches are eliminated entirely
2. **Compile-time auditability** — You can trace the policy tag to see exactly which code runs
3. **noexcept compatibility** — The compiler knows at compile time whether the selected branch can throw

The constraint that all policies must be known at compile time is acceptable because:
- FAT-P is designed for static configuration
- Runtime policy switching is an anti-pattern in performance-critical code
```

### 6. Consequences

What follows from this decision:

```markdown
## Consequences

### Positive
- No virtual dispatch overhead
- Full noexcept analysis at compile time
- Policies are self-documenting (you can grep for policy tag usage)

### Negative
- Adding a new policy requires modifying the if-constexpr chain
- Runtime policy selection is not supported (by design)

### Obligations
- Any new raiser must be added to the if-constexpr dispatch
- Documentation must explain policy selection clearly
```

### 7. Status

```markdown
## Status

**Status:** Final
**Decided:** 2025-06-15
**Last reviewed:** 2025-12-01
```

## Design Note Template

```markdown
---
doc_id: DN-0000
doc_type: "Design Note"
title: "[Decision Name]"
fatp_components: ["..."]
topics: ["..."]
constraints: ["..."]
cxx_standard: "C++17"
last_verified: "YYYY-MM-DD"
audience: ["C++ developers", "AI assistants"]
status: "final"
---

# Design Note - [Decision Name]

**Status:** draft | final | obsolete
**Decided:** YYYY-MM-DD
**Last reviewed:** YYYY-MM-DD

## Decision

[One sentence stating the decision]

---

## Context

[What situation prompted the decision]

---

## Constraints

1. ...
2. ...

---

## Options Considered

### Option A: [Name]

[Description]

**Pros:** ...
**Cons:** ...

### Option B: [Name]

[Description]

**Pros:** ...
**Cons:** ...

### Option C: [Name] (Selected)

[Description]

**Pros:** ...
**Cons:** ...

---

## Decision Rationale

[Why this option was chosen]

---

## Consequences

### Positive
- ...

### Negative
- ...

### Obligations
- ...
```

## Design Note Checklist

- [ ] YAML header present
- [ ] Status clearly stated (draft/final/obsolete)
- [ ] Decision stated in one sentence
- [ ] Context explains what prompted the decision
- [ ] Constraints list what limited options
- [ ] At least 2-3 options considered with pros/cons
- [ ] Decision rationale explains why this option
- [ ] Consequences section with positive, negative, and obligations

## Common Failure Mode

Writing a design note when you need a full Companion Guide. If the "note" exceeds 2-3 pages, consider splitting or upgrading to a Companion Guide.

---

# 9. BENCHMARK RESULTS

## Intent

Present benchmark data in a structured, comparable format.

Benchmark Results documents are **data presentations**, not methodology tutorials. They show:
- What was measured
- Under what conditions
- What the numbers are
- What caveats apply

Methodology explanation belongs in a Case Study or Handbook.

## Required Sections

### 1. Header

```markdown
# Benchmark Results - [Component]

**Platform:** Ubuntu 24.04, GCC 13.2 -O3 -march=native, Intel i9-13900K
**Date:** December 2025
**Source:** `benchmark_Component.cpp`
```

### 2. Summary

2-3 sentences: what was measured, key findings.

```markdown
## Summary

SmallVector<T, N> outperforms std::vector by 2-4x for small collections that fit in the inline buffer. Performance converges once both use heap storage.
```

### 3. Test Environment

| Property | Value |
|----------|-------|
| OS | Ubuntu 24.04 |
| Compiler | GCC 13.2 -O3 -march=native |
| CPU | Intel i9-13900K @ 5.8 GHz |
| RAM | 64 GB DDR5-5600 |

### 4. Benchmark Sections

Each section needs:
- **What:** One sentence describing what's measured
- **Table:** With Size/Condition, Component, Baseline, Ratio/Speedup
- **Notes:** Any caveats or explanations

```markdown
## Core Operations (N=10,000)

**What:** Insert, find, erase on pre-built container

| Operation | SmallVector | std::vector | Ratio |
|-----------|-------------|-------------|-------|
| push_back | 3.2 ns | 4.1 ns | 1.3x |
| operator[] | 0.8 ns | 0.9 ns | 1.1x |
| iteration | 0.3 ns/elem | 0.3 ns/elem | 1.0x |

**Notes:** Timing excludes container construction.
```

### 5. Caveats

At the end, list limitations:

```markdown
## Caveats

- Results are single-threaded only
- Custom allocators not tested
- String keys may show different characteristics than int keys
- Windows results pending (Linux only in this version)
```

## Section Types

### Core Operations
Basic insert, find, erase, iterate.

### Scaling Behavior
How performance changes with size.

### Competitor Comparison
Performance across implementations (tsl, absl, boost, std).

### Feature-Specific
Benefit of specific features (inline buffer, SIMD, etc.).

## Table Standards

### Required Columns

| Column | Purpose |
|--------|---------|
| Size/Condition | What varies |
| Your implementation | Measured result |
| Baseline (std::) | Comparison point |
| Ratio or Speedup | Quick interpretation |

### Number Formatting

| Good | Bad |
|------|-----|
| 3.2 ns | 3.234523 ns |
| 1.3x | 1.28x |
| 2.9 ms | 2900000 ns |

- 1-2 significant figures for ratios
- Appropriate units (ns, μs, ms)
- No false precision

## Benchmark Results Template

```markdown
---
doc_id: BR-0000
doc_type: "Benchmark Results"
title: "[Component]"
fatp_components: ["[Component]"]
topics: ["performance", "benchmarking"]
cxx_standard: "C++17"
last_verified: "YYYY-MM-DD"
audience: ["C++ developers", "AI assistants"]
status: "draft"
---

# Benchmark Results - [Component]

**Platform:** [OS], [Compiler] [Flags], [CPU]
**Date:** [Month Year]
**Source:** `benchmark_[Component].cpp`

---

## Summary

[2-3 sentences: what was measured, key findings]

---

## Test Environment

| Property | Value |
|----------|-------|
| OS | ... |
| Compiler | ... |
| CPU | ... |
| RAM | ... |

---

## [Benchmark Section 1]

**What:** [One sentence]

| Size | [Component] | std:: | Speedup |
|------|-------------|-------|---------|
| ... | ... | ... | ... |

**Notes:** ...

---

## [Benchmark Section 2]

...

---

## Caveats

- ...
- ...
```

## Benchmark Results Checklist

- [ ] YAML header present
- [ ] Platform and date specified
- [ ] Source file referenced
- [ ] Summary in 2-3 sentences
- [ ] Test environment table
- [ ] Each section has "What:" description
- [ ] Tables have Size/Condition, Ours, Theirs, Ratio
- [ ] Numbers have appropriate precision
- [ ] Notes explain non-obvious results
- [ ] Caveats section lists limitations

## What NOT to Include

| Avoid | Why |
|-------|-----|
| Raw benchmark output | Present processed results |
| Methodology explanation | That's the Case Study/Handbook |
| Teaching about benchmarking | That's the Handbook |
| Implementation details | That's the Companion Guide |
| Usage instructions | That's the User Manual |

## Common Failure Mode

Including too much explanation. Benchmark Results are data presentations. If you're explaining *why* the numbers are what they are, you need a Case Study.

---

# Cross-Document Interoperability Rules

Because AI is a primary client, cross-doc consistency matters.

## 1. Cross-Link with Stable Names

Prefer linking to documents by **filename** (type-prefixed) and, when possible, mention `doc_id`.

```markdown
See [Case Study - The Slow Miss](CS-HASHMAP-001) for the full investigation.
```

## 2. Use Stable Section Headings for Retrieval

Prefer these headings across all documents:
- "Scope"
- "Not covered"
- "Prerequisites"
- "The Hidden Constraint"
- "Guarantees / Non-Guarantees"
- "Where it loses"
- "What To Do Now"
- "Mechanical Audit Checklist"
- "Glossary"
- "Caveats"

## 3. Avoid Ambiguous Pronouns in Critical Sections

Use explicit nouns:
- "this macro expansion"
- "this destructor"
- "this overflow path"

Not just "this" or "it" without a referent.

## 4. Define Terms Once, Then Reuse

If you define "control-flow barrier," keep using that phrase. Don't use five synonyms.

## 5. Separate Facts from Hypotheses

Use explicit labels:
- **Fact:** backed by evidence/audit/measurement
- **Hypothesis:** plausible but not proven

---

# Publishing Checklist (Universal)

## All Documents

- [ ] YAML header present and accurate
- [ ] H1 title matches filename prefix (`# <Doc Type> - <Title>`)
- [ ] Scope + Not covered + Prerequisites
- [ ] Facts vs Hypotheses labeled
- [ ] Verbatim/excerpt/pseudocode correctly labeled
- [ ] Debug vs Release differences called out (if relevant)
- [ ] Glossary exists if > ~1200 lines or many terms introduced
- [ ] No banned vocabulary in prose
- [ ] Active voice throughout

## Document-Specific Additions

See individual checklist sections above.

---

# Notes on Evolving Documents

When a document grows too large:
- **Split by failure mode** into multiple Case Studies
- **Split by discipline area** into multiple Handbooks
- **Extract pattern sections** into Pattern Guides
- **Extract background material** into Foundations

Prefer many small, high-signal docs over one mega-doc.

---

*FAT-P Teaching Documents Style Guide (Enhanced Edition) v1.0 — December 2025*
