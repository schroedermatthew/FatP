# Fat-P Human Guidance Document

## Purpose

This document is for **human maintainers** of the Fat-P library. It explains:
1. What the governance system is
2. What you should NOT change
3. What you CAN safely modify
4. How to work effectively with AI assistants

---

## The Governance System: What It Is

Fat-P uses a 7-document governance suite that functions as a **constitution for AI-assisted development**. The documents constrain AI behavior by:

- Banning vague vocabulary (forces mechanism-specific language)
- Requiring explicit templates (increases output determinism)
- Mandating caveats sections (suppresses overconfident claims)
- Enforcing precedence rules (resolves conflicts without judgment calls)

**Key insight:** These documents work as a "prompt compiler" -- they shape what AIs can generate, not just what they should generate.

**The 7 Documents:**

| Document | Purpose |
|----------|---------|
| Development Guidelines | THE authority for code, docs, and AI behavior |
| User Manual Style Guide | Template for user-facing documentation |
| Companion Guide Style Guide | Template for design rationale documentation |
| Overview Style Guide | Template for orientation documentation |
| Test Suite Style Guide | Template for test code |
| Benchmark Results Style Guide | Template for benchmark result presentation |
| Human Guidance (this) | Instructions for humans |

---

## What You Should NOT Change

These are load-bearing elements. Weakening them will measurably degrade AI output quality.

### 1. Authority Hierarchy

```
HIGHEST: Development Guidelines (always wins)
PRIMARY: Domain Style Guides
REFERENCE: Human Guidance
```

**Do not:** Create exceptions, add "soft" overrides, or allow domain guides to contradict Development Guidelines.

### 2. Banned Vocabulary (for Documentation)

| Banned | Required Replacement |
|--------|---------------------|
| Fast | Zero-allocation, O(1), cache-local |
| Safe | Bounds-verified, lifetime-tracked |
| Efficient | Constant-time, single-pass |
| Simple | Minimal API, single-header |
| Powerful | Composable, policy-based |

**Do not:** Add exceptions "just this once." The vocabulary ban is the single most effective AI control mechanism.

**Note:** These bans apply to documentation prose, not component names. `FastHashMap` is a valid name.

### 3. Honesty Requirements

- "Where it loses" sections are mandatory
- Caveats sections are mandatory
- Benchmark methodology (round-robin, median) is mandatory

**Do not:** Remove these requirements to make documentation "cleaner" or "more positive."

### 4. Template Structures

The Four-Part Arc (Problems -> Solutions -> Case Studies -> Foundations) and other templates exist to constrain AI generation paths.

**Do not:** Make templates "flexible" or "optional."

### 5. Test Namespace Pattern

```cpp
namespace fat_p::testing::componentns  // Named nested namespace - REQUIRED
```

**Do not:** Allow anonymous namespaces in test headers. This prevents ODR violations.

---

## What You CAN Safely Modify

### 1. Specific Examples

You can update, replace, or add examples without breaking the system.

### 2. Domain Tables

The Companion Guide's domain table (HPC, Serialization, etc.) can be extended for new component categories.

### 3. Benchmark Environment Details

Hardware specs, compiler versions, and build flags can be updated as your environment changes.

### 4. Adjective Definitions

You can ADD new semantic adjectives to the naming table (e.g., "Concurrent" = thread-safe operations), but do not remove existing definitions.

### 5. Checklist Items

You can add new checklist items. Be cautious about removing existing ones.

---

## Policy-Based Design: Clarification

**Policy-based design is NOT a hard requirement.**

| Component Type | Policy Appropriate? |
|----------------|:------------------:|
| StableHashMap (needs custom hash/allocator) | Yes |
| SmallVector (fixed behavior) | No |
| CircularBuffer (fixed behavior) | No |
| ThreadPool (might need scheduling policies) | Maybe |

**Rule of thumb:** Add policies when users legitimately need to customize hash functions, allocators, or comparison predicates. Do not add policies "for future flexibility."

**Concurrency is NOT built-in by default.** Basic containers should be single-threaded. Concurrent variants are separate components (e.g., `ConcurrentQueue` vs `Queue`).

---

## Working with AI Assistants

### What AIs Need From You

1. **Complete files** -- AIs should flag truncation, but provide complete inputs when possible
2. **Explicit scope** -- "Review this component" vs "Review and fix this component"
3. **Confirmation of context** -- If you've uploaded guidelines, confirm which version

### What to Expect From AIs

AIs following these guidelines will:
- Use specific vocabulary (never "fast" or "safe" in docs)
- Include caveats and limitations
- Follow template structures
- Ask for missing dependencies rather than guess
- Provide complete code (no truncation)

### Red Flags (AI Not Following Guidelines)

- Uses banned vocabulary without mechanism explanation
- Skips "Where it loses" section
- Provides truncated code
- Claims to have compiled without actually doing so
- Uses anonymous namespaces in test code

---

## Versioning and Changes

### Current Version

All documents are v2.0. There is no formal versioning strategy yet.

### Recommended Change Process

1. Propose changes in a separate document
2. Identify which load-bearing elements (if any) are affected
3. If load-bearing elements change, require explicit justification
4. Update all affected documents simultaneously to prevent drift

---

## Quick Reference: The Rules

### Code Rules (Development Guidelines)
- C++17 minimum, header-only
- No external dependencies
- No exceptions crossing API boundaries
- `#pragma once` for include guards
- Lines <= 100 columns

### Naming Rules
- Files: `PascalCase.h`, `test_Component.h`
- Types: `PascalCase`
- Functions: `camelCase`
- Constants: `SCREAMING_SNAKE_CASE`
- Members: `mPascalCase`

### Test Rules
- Nested namespace: `fat_p::testing::componentns`
- Use `TEST_CASE()` and `RUN_TEST_NS()` macros
- Every assertion has a message
- Benchmarks use `DoNotOptimize()`

### Documentation Rules
- Explain "why" before "how"
- Tables for comparisons
- Diagrams for complex flows
- No forbidden phrases ("This class provides...", "In order to...")

---

# Appendix A: AI Capability Estimates

This appendix consolidates assessments of each AI's capabilities for Fat-P development work. Use this to decide which AI to assign to which task.

---

## Summary Matrix

### Compilation Capabilities

| AI | Can Compile C++ | Can Execute Binaries |
|----|:---------------:|:--------------------:|
| **Claude** | Yes (GCC/G++ in Linux container) | Yes |
| **ChatGPT** | No (description-based only) | No |
| **Gemini** | No (static analysis only) | No |
| **Grok** | Yes (claimed, stateful REPL) | Yes (claimed) |

### Best-Use Recommendations

| AI | Best For | Worst For |
|----|----------|-----------|
| **Claude** | Template compliance, code verification, structured review | SIMD, platform-specific, very large files |
| **ChatGPT** | Conceptual framing, recommendations, failure-mode analysis | Code execution, implementation details |
| **Gemini** | Technical pattern identification, concise analysis | Comprehensive coverage, meta-tasks, task completion |
| **Grok** | Exhaustive coverage, gap analysis, research | Conciseness, prioritization |

### Trust Levels for Fat-P Work

| AI | Trust Level | Rationale |
|----|:-----------:|-----------|
| **Claude** | High | Demonstrated compilation, strong template adherence |
| **ChatGPT** | Medium-High | Best framing, but no execution capability |
| **Gemini** | Medium | Good technical specifics, but task completion issues |
| **Grok** | Medium-High | Claims execution, exhaustive coverage, but verbose |

---

## Operational Recommendations

### For Code Generation/Review
1. **Primary:** Claude (can compile and verify)
2. **Secondary:** Grok (claims compilation, exhaustive)
3. **Tertiary:** ChatGPT (conceptual review only)

### For Documentation Generation
1. **Primary:** Claude or ChatGPT (both strong at templates)
2. **Secondary:** Grok (comprehensive but verbose)
3. **Tertiary:** Gemini (concise but may miss sections)

### For Conceptual/Architectural Review
1. **Primary:** ChatGPT (best framing, latent structure identification)
2. **Secondary:** Claude (good structure, may be human-centric)
3. **Tertiary:** Grok (exhaustive but process-heavy)

### For Technical Pattern Detection
1. **Primary:** Gemini (best at ODR, anonymous namespace issues)
2. **Secondary:** Claude (good but less focused)
3. **Tertiary:** Grok (catches edge cases but buries them)

---

## Guideline Refresh Procedures

Each AI has different drift characteristics. Use these to decide when to re-anchor.

### Claude

**Re-present guidelines:** Never (they're in project knowledge)

**When to prompt re-read:**
- After ~20 exchanges on the same topic
- If output looks wrong
- When starting a task that's been discussed but not executed

**Prompt format:** "Re-read [document name] before proceeding."

### ChatGPT

**Mental model:** Strong short-term adherence, weak long-term anchoring across task shifts.

**Must re-anchor after:**
- Any reset
- Task type changes (code review -> documentation)
- Introducing new artifact class
- Precision-critical work (concurrency, lifetime, ABI, ODR)

**Minimal re-anchor:**
> "This task must follow Fat-P Development Guidelines. Banned vocabulary applies. No guessing or invented behavior. Flag truncated input."

### Gemini

**Mental model:** Less about timing, more about contextual state.

**Three triggers:**
1. **Stateless Reset:** Re-assert at start of every new task/component
2. **Hallucination Trap:** If banned term used, stop and re-anchor immediately
3. **Capability Drift Check:** Periodically verify it can see attached files

### Grok

**Mental model:** Retains context within session, but needs anchoring at session start.

**Best practices:**
- Start with short anchor: "Continue Fat-P work. Development Guidelines are authoritative."
- Use targeted citations when correcting
- Attach core files once per conversation
- Flag drift early

---

## Reset Protocol

**Human-initiated only.** No AI should auto-reset.

**Completeness criterion:** After a human-initiated reset, if any AI produces new essential findings, the artifact is not complete.

**Recommended process:**
1. Human initiates reset (new conversation or explicit instruction)
2. AI reviews fresh without prior context
3. Findings compared to pre-reset review
4. Zero new essential findings = complete
5. New findings = iterate

---

## Individual AI Assessments

### Claude

**Self-Assessment:**

| Property | Value |
|----------|-------|
| Context window | ~200,000 tokens |
| Truncation handling | Will flag explicitly, will not guess |

**Strengths:**
- Template compliance -- excellent at following explicit structures
- Vocabulary discipline -- strong at avoiding banned terms
- Code compilation -- can actually compile and verify
- Documentation -- strong narrative following style guides

**Limitations:**
- SIMD intrinsics -- limited optimization knowledge
- Concurrency bugs -- can miss subtle race conditions
- Performance optimization -- cannot benchmark meaningfully
- Platform coverage -- Linux only
- Very large files -- degrades on >10,000 lines

**Cross-AI Assessment:**
- ChatGPT: "Excellent at structured reasoning, but can overfit to initial framing"
- Gemini: "Superior ability to adhere to rigid guidelines without drifting"
- Grok: "Best at systematic comparisons, occasionally human-centric"

---

### ChatGPT

**Self-Assessment:**

| Property | Value |
|----------|-------|
| Context window | Variable by model |
| Truncation handling | Will flag, will not guess |

**Strengths:**
- Strong at reframing systems correctly
- Good at identifying latent structure ("prompt compiler")
- Good at failure-mode analysis

**Limitations:**
- Cannot compile/run code
- Requires explicit resets to avoid anchoring
- Initially too human-centric unless corrected

**Cross-AI Assessment:**
- Claude: "Best conceptual framing, but tends toward abstraction over implementation"
- Gemini: "Fast and reliable, may oversimplify complex reasoning"
- Grok: "Most insightful framing, sometimes drifts into philosophical prose"

---

### Gemini

**Self-Assessment:**

| Property | Value |
|----------|-------|
| Context window | ~32,000 tokens |
| Truncation handling | Will flag and stop |

**Strengths:**
- Very good at local technical correctness
- Strong at identifying subtle C++ pitfalls
- Concise and precise

**Limitations:**
- Cannot compile or run code
- Relies on static analysis and logic verification
- Shortest context window

**Cross-AI Assessment:**
- Claude: "Best at specific technical patterns, but tends to be shallow when depth required"
- ChatGPT: "Concise but weak at meta-tasks, frequently misses what's being asked"
- Grok: "Good at surfacing low-level issues, weakest task adherence in meta-reviews"

---

### Grok

**Self-Assessment:**

| Property | Value |
|----------|-------|
| Context window | Large (no hard limit observed) |
| Truncation handling | Will always flag, refuse to guess |

**Strengths:**
- Strong template adherence and checklist generation
- Full code execution and verification
- Deep static analysis of header-only ODR issues
- Precise vocabulary enforcement

**Limitations:**
- No persistent workspace across conversations
- Cannot install external packages beyond pre-installed set
- Internet access limited to specific tools

**Cross-AI Assessment:**
- Claude: "Most exhaustive coverage, but verbose -- buries key insights in detail"
- ChatGPT: "Good gap enumeration, but over-process oriented"
- Gemini: "Direct critiques, but reasoning on complex C++ can be less consistent"

---

*Fat-P Human Guidance Document v2.0 -- December 2025*
