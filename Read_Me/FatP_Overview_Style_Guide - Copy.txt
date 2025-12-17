# Fat-P Overview Document Style Guide

## Purpose

This guide ensures consistent, compelling documentation for all fat_p library components. Every overview document should position fat_p as **architecturally superior** to both standard library equivalents and competing implementations—not merely as a temporary compatibility shim.

---

## The Three-Note Checklist

Every component description MUST hit these three notes:

### 1. Permanence
This is NOT a temporary fix until compiler upgrades arrive. This IS the solution.

**Wrong:** "Until C++23 is available, use Expected..."
**Right:** "Expected provides policy-based error handling that std::expected will never offer."

### 2. Specialization  
The standard is generic; fat_p is HPC-tuned.

**Wrong:** "Similar to std::vector but with inline storage."
**Right:** "Transforms allocation-bound loops into compute-bound operations through stack-local storage."

### 3. Control
The standard is one-size-fits-all; fat_p is policy-based.

**Wrong:** "Handles overflow safely."
**Right:** "Allows compile-time selection of overflow behavior—throw, saturate, or return Expected—without virtual dispatch overhead."

---

## Vocabulary Standards

### Banned Terms → Replacements

| ❌ Banned | ✅ Replacement | Rationale |
|-----------|---------------|-----------|
| Polyfill | Wrapper, Shim, Compatibility layer | Web-centric; wrong domain |
| Backport | Architectural superset, Enhanced implementation | Implies temporary |
| Similar to | Inspired by, Extends beyond | Implies equivalence |
| Safe | Hardened, Crash-proof, Transactional | Stronger verbs |
| Handles | Guarantees, Enforces, Prevents | More definitive |
| Fast | Zero-overhead, Branchless, Cache-optimal | Specific mechanisms |
| Simple | Minimal, Focused, Single-header | Avoids condescension |

### Power Phrases

Use these to describe fat_p advantages:

- "Architectural superset of the standard feature"
- "Production-hardened implementation"
- "Zero-overhead abstraction"
- "Compile-time policy resolution"
- "Pointer-discriminating storage" (for SmallVector-style components)
- "Deterministic memory behavior"
- "No virtual dispatch overhead"
- "Transforms [X]-bound code to [Y]-bound code"

---

## Document Structure

### 1. Executive Summary (3-4 sentences)

**Formula:**
1. What it is (one sentence)
2. The architectural advantage (one sentence)  
3. Key differentiator from standard/alternatives (one sentence)
4. Quantified benefit if available (one sentence)

**Example:**
> SmallVector is a hybrid stack/heap container that eliminates heap allocation for small element counts. Unlike naive implementations using boolean flags, it employs pointer-discriminating storage to achieve branchless element access. This architectural choice—inspired by LLVM but dependency-free—transforms allocation-bound loops into compute-bound operations, delivering 2-10x speedup for small collections.

### 2. The Problem Domain

**Structure:**
- "What Goes Wrong Without It" — Show broken/naive code
- Problem table with **Impact** column (not just "Issue")
- "The Standard's Limitation" — Why std:: doesn't solve it

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

**Format:** Numbered subsections with code examples.

**Each feature must answer:**
1. What does it do?
2. How does it achieve zero overhead?
3. When would you use it vs. alternatives?

### 5. Why Not Alternatives? (Critical Section)

**This replaces generic comparison tables.**

Structure as exclusionary criteria:

| If You Need... | Why Not [Alternative] | Fat-P Advantage |
|----------------|----------------------|-----------------|
| Zero dependencies | LLVM SmallVector requires LLVM headers | Single header, STL only |
| Standard allocators | Boost.Container uses custom allocator model | std::allocator compatible |
| No metaprogramming | Folly uses heavy template machinery | Minimal template instantiation |

**Key insight:** Position fat_p as the ONLY option when combining multiple requirements.

### 6. The "Forever Stuck" Reality (New Section)

Address compiler lock-in explicitly:

> **Compiler Reality Check:** Scientific clusters often run RHEL 7/8 with GCC 7.x for driver compatibility. Even when C++23 offers similar features, your codebase may be contractually locked to C++17 for years. Fat_p bridges this gap permanently—not as a temporary shim, but as an architecturally superior solution that remains valuable even after compiler upgrades.

### 7. Performance Characteristics

**Must include:**
- Specific mechanisms (not just "fast")
- Benchmark methodology notes
- When fat_p loses (intellectual honesty builds trust)

**Example:**
> **Where fat_p loses:** For collections consistently exceeding inline capacity, std::vector with reserve() matches performance. SmallVector's advantage is statistical—it wins when most instances stay small.

### 8. Integration Points

Show how this component connects to the fat_p ecosystem:

```
SmallVector
    ↓ uses
enforce.h (bounds checking)
    ↓ used by
Signal.h (zero-alloc slot storage)
FatPJsonLite.h (inline JSON arrays)
```

### 9. Final Assessment

**Formula:**
1. Restate the architectural advantage
2. List the three pillars: Permanence, Specialization, Control
3. One-sentence verdict

---

## Tone Guidelines

### Do:
- Use active voice: "SmallVector eliminates allocation" not "Allocation is eliminated"
- Be specific: "4 inline elements" not "several elements"
- Show mechanism: "pointer comparison" not "clever trick"
- Acknowledge tradeoffs: "Loses to std::vector when..." builds trust

### Don't:
- Apologize: "This is just a simple wrapper" ❌
- Hedge: "May provide some performance benefits" ❌
- Over-promise: "Always faster than everything" ❌
- Use web terminology: "Polyfill", "shim for browsers" ❌

---

## Code Example Standards

### Good Example (shows the WHY):
```cpp
// Without SmallVector: 3 heap allocations in hot loop
for (int i = 0; i < 1000000; ++i) {
    std::vector<int> temp;  // Allocation #1
    temp.push_back(a);       // Possible realloc
    temp.push_back(b);       // Possible realloc
    process(temp);
}  // Deallocation

// With SmallVector: 0 heap allocations
for (int i = 0; i < 1000000; ++i) {
    SmallVector<int, 4> temp;  // Stack storage
    temp.push_back(a);          // No alloc
    temp.push_back(b);          // No alloc
    process(temp);
}  // No dealloc
```

### Bad Example (just shows syntax):
```cpp
SmallVector<int, 4> v;
v.push_back(1);
v.push_back(2);
```

---

## Template

```markdown
# [Component]: A Fat-P Library Showcase

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

[Repeat for each major feature]

---

## Why Not Alternatives?

| If You Need... | Why Not [Alt] | Fat-P Advantage |
|----------------|---------------|-----------------|
| [Requirement] | [Alt's limitation] | [Fat-P solution] |

---

## The "Forever Stuck" Reality

[Address compiler lock-in: RHEL, CUDA drivers, contractual C++ version limits]

---

## Performance Characteristics

| Operation | Complexity | Mechanism |
|-----------|------------|-----------|
| [Op] | O(...) | [How it achieves this] |

### Where Fat-P Wins
[Specific scenarios]

### Where Fat-P Loses (Honesty Builds Trust)
[When alternatives match or beat fat_p]

---

## Integration Points

```
[Component]
    ↓ uses
[Dependency 1]
    ↓ used by
[Dependent component 1]
```

---

## Final Assessment

[Component] delivers on the fat_p promise:

1. **Permanence:** [Why this isn't just waiting for C++2x]
2. **Specialization:** [HPC-specific advantage]
3. **Control:** [Policy-based customization]

[One-sentence architectural verdict]

---

*[Component].h — Fat-P Library*
```

---

## Checklist Before Publishing

- [ ] No instances of "polyfill" or "backport"
- [ ] Executive summary states mechanism, not just benefit
- [ ] "Why Not Alternatives?" section uses exclusionary criteria
- [ ] Performance section acknowledges where fat_p loses
- [ ] Code examples show WHY, not just syntax
- [ ] Three-note checklist (Permanence, Specialization, Control) addressed
- [ ] Compiler lock-in reality acknowledged
- [ ] Active voice throughout
- [ ] Specific numbers where possible (not "fast" but "2-10x")

---

*Fat-P Documentation Standards v1.0*
