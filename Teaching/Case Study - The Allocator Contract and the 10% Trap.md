---
doc_id: CS-SMALLVECTOR-002
doc_type: "Case Study"
title: "The Allocator Contract and the 10% Trap"
fatp_components: ["SmallVector"]
topics: ["allocator_traits", "construct bypass", "PMR", "polymorphic allocator", "benchmark-driven development", "microbenchmark trap"]
constraints: ["allocator contract", "polymorphic memory resources", "scoped_allocator_adaptor", "custom allocator instrumentation"]
cxx_standard: "C++17"
std_equivalent: "std::inplace_vector"
std_since: "C++26"
boost_equivalent: "Boost.Container small_vector"
build_modes: ["Release"]
last_verified: "2026-01-14"
audience: ["C++ developers", "library maintainers", "performance engineers", "AI assistants"]
status: "reviewed"
---

# Case Study - The Allocator Contract and the 10% Trap

## Scope

This case study examines a critical design decision in SmallVector: whether to bypass `std::allocator_traits<A>::construct()` for trivially copyable types. We analyze why this "optimization" produces measurable benchmark wins but silently breaks C++17's polymorphic memory resources and custom allocator instrumentation.

## Not covered

- SmallVector's inline storage mechanism (see Case Study - SmallVector for that)
- General allocator design patterns
- Memory pool implementations
- Boost's full container library design

## Prerequisites

- C++17 proficiency (allocator model, `std::allocator_traits`)
- Familiarity with custom allocators and their use cases
- Understanding of placement new vs allocator-aware construction
- Basic awareness of benchmark methodology
- PMR familiarity helpful but not required (see [Appendix C](#appendix-c-why-pmr-exists) for background)

## Case Study Card

**Problem:** After optimizing SmallVector's fast path to parity with `std::vector`, boost::small_vector remained ~10% faster  
**Constraint:** The C++ allocator model requires `allocator_traits::construct()` to be called for all object construction, not bypassed  
**Symptom:** Measured 1.20 ns/op vs boost's 1.09 ns/op in fast-path throughput benchmarks  
**Root cause:** boost bypasses `allocator_traits::construct()` for trivially copyable types, using direct placement new  
**Fix pattern:** Not a bug—fat_p's behavior is correct. The "fix" is understanding why the 10% gap is acceptable  
**FAT-P components used:** SmallVector, FatPBenchmarkUtils  
**Build-mode gotchas:** None (same behavior in Debug and Release)  
**Guarantees:** Full allocator contract compliance; PMR and custom allocators work correctly  
**Non-guarantees:** Does not guarantee parity with libraries that violate the allocator contract

---

## ⚠️ Before You Read Further: The Optimization That Breaks Everything

If you're tempted to "optimize" your container's construction path like this:

```cpp
// THE TRAP: Bypass allocator_traits for "trivially copyable" types
template<typename... Args>
void emplace_back(Args&&... args) {
    if constexpr (std::is_trivially_copyable_v<T>) {
        // "Optimization": skip allocator, use direct placement new
        ::new (static_cast<void*>(slot)) T(std::forward<Args>(args)...);  // 10% faster!
    } else {
        AllocTraits::construct(mAllocator, slot, std::forward<Args>(args)...);
    }
}
```

**Stop.** This 10% microbenchmark win silently breaks:
- **C++17 PMR containers** — nested containers won't receive their allocator
- **`scoped_allocator_adaptor`** — the entire point of the adaptor is bypassed
- **Custom allocators with instrumentation** — your allocation tracking is now wrong
- **Allocators with side effects in `construct()`** — your debugging infrastructure lies to you

The benchmark shows 10% improvement. The production system silently corrupts state.

---

## Table of Contents

- [Part I — The Problems](#part-i--the-problems)
  - [The Obvious Approach](#the-obvious-approach)
  - [The Hidden Constraint](#the-hidden-constraint)
  - [The Symptoms](#the-symptoms)
  - [The Cost](#the-cost)
- [Part II — The Solutions](#part-ii--the-solutions)
  - [Understanding allocator_traits::construct](#understanding-allocator_traitsconstruct)
  - [Why PMR Requires the Full Protocol](#why-pmr-requires-the-full-protocol)
  - [The Guarantees Table](#the-guarantees-table)
- [Part III — The Investigation](#part-iii--the-investigation)
  - [Context: The Optimization Journey](#context-the-optimization-journey)
  - [The Optimization Work](#the-optimization-work)
  - [The Measurement Problem](#the-measurement-problem)
  - [The New Benchmark](#the-new-benchmark)
  - [The Results and the Gap](#the-results-and-the-gap)
  - [Hypothesis: Where Does the Gap Come From?](#hypothesis-where-does-the-gap-come-from)
  - [Verification: What Boost Actually Does](#verification-what-boost-actually-does)
- [Part IV — Foundations](#part-iv--foundations)
  - [Design Rationale](#design-rationale)
  - [Rejected Alternatives](#rejected-alternatives)
  - [The Broader Lesson: Optimizing for the Right Metric](#the-broader-lesson-optimizing-for-the-right-metric)
  - [The Real Performance Story](#the-real-performance-story)
- [Design Rules to Internalize](#design-rules-to-internalize)
- [What To Do Now](#what-to-do-now)
- [Appendix A: Benchmark Methodology](#appendix-a-benchmark-methodology)
- [Appendix B: The PMR Test](#appendix-b-the-pmr-test)
- [Appendix C: Why PMR Exists](#appendix-c-why-pmr-exists)
- [Appendix D: The Full Optimization History](#appendix-d-the-full-optimization-history)
- [References](#references)
- [Appendix D: Full Optimization History](#appendix-d-full-optimization-history)

---

## Part I — The Problems

### The Obvious Approach

SmallVector exists to solve a specific problem: avoid heap allocation for small, temporary collections. The benchmarks prove it works—5.7x to 17.6x faster than `std::vector` for small N. (For the full analysis of why allocation avoidance matters and the memory hierarchy effects at play, see *Case Study - SmallVector* [5].) But after achieving that primary goal, a natural question arises: *is the implementation as efficient as it could be?*

During optimization work on SmallVector's `emplace_back` fast path (detailed in Part III), we implemented several improvements: separating fast and slow paths, reordering memory layout for cache efficiency, eliminating redundant loads. After all that work, we needed to measure the results. The standard benchmark comparing against allocating `std::vector` couldn't help—the allocation overhead difference would swamp any fast-path improvement.

So we built a new benchmark: SmallVector vs `std::vector` with `reserve()`, both operating on pre-allocated storage. This isolates the fast-path code from allocation effects. The results showed we'd achieved parity with `std::vector`—the theoretical best case for an allocator-compliant container.

But then we added boost::small_vector to the comparison. And boost was 10% faster.

The temptation is immediate: close the gap. If boost can do it, why can't we? The investigation revealed boost's approach—bypass `allocator_traits::construct()` for trivially copyable types and use direct placement new instead. For `std::allocator<T>`, the `construct()` function is specified to do exactly what placement new does. So why pay for the indirection? The compiler will inline it anyway, right?

This reasoning has led boost::container down this path. And the benchmarks look great.

### The Hidden Constraint

The C++ allocator model isn't just about memory allocation. It's a protocol with three distinct operations that must be called in sequence:

1. **`allocate()`** — obtain raw memory
2. **`construct()`** — create an object in that memory
3. **`destroy()`** + **`deallocate()`** — tear down and release

The critical insight is that `construct()` is not equivalent to placement new for all allocators. It's only equivalent for `std::allocator<T>`. For custom allocators, `construct()` is where custom behavior lives.

The C++17 standard introduced `std::pmr::polymorphic_allocator`, and its `construct()` does something fundamentally different. (For background on why PMR exists and what problem it solves, see [Appendix C](#appendix-c-why-pmr-exists).) Here's what the standard specifies (simplified from [allocator.uses.construction] [2]):

```cpp
// What std::allocator::construct() does:
template<class U, class... Args>
void construct(U* p, Args&&... args) {
    ::new (static_cast<void*>(p)) U(std::forward<Args>(args)...);
}

// What pmr::polymorphic_allocator::construct() does:
template<class U, class... Args>
void construct(U* p, Args&&... args) {
    if constexpr (uses_allocator_v<U, polymorphic_allocator>) {
        // Propagate allocator to nested container!
        ::new (p) U(std::forward<Args>(args)..., *this);
    } else {
        ::new (p) U(std::forward<Args>(args)...);
    }
}
```

The difference is everything. When you construct a `std::pmr::vector<int>` inside a `std::pmr::vector<std::pmr::vector<int>>`, the outer container's allocator must be passed to the inner container. That's what `construct()` does. Bypass it, and the inner container silently gets the default allocator instead.

### The Symptoms

The symptoms of this bug are insidious because they're silent:

**Silent allocator mismatch:** Your outer container uses a custom memory pool. Your inner containers silently use the default heap. Everything "works" until you run out of pool memory while the heap is nearly empty.

**Corrupted allocation tracking:** Your custom allocator counts constructions and destructions for leak detection. The counts don't match. You spend days debugging your allocator before realizing the container never called `construct()`.

**Memory fragmentation in production:** You designed a memory architecture with pools for different object lifetimes. The containers ignore your design and allocate everywhere. Your carefully tuned system degrades under load.

**Test passes, production fails:** Unit tests use `std::allocator` (where the bypass is correct). Production uses PMR for memory isolation. The bug only manifests under production conditions.

### The Cost

The cost of violating the allocator contract is difficult to quantify because the failures are silent and often delayed. But consider this scenario:

A game engine uses PMR to isolate per-frame allocations. Each frame gets a monotonic buffer that's bulk-deallocated at frame end. The engine uses boost::small_vector with PMR allocators for temporary collections during rendering.

Because boost bypasses `construct()` for trivially copyable types, any `small_vector<small_vector<int>>` (nested vectors) silently allocates the inner vectors from the global heap instead of the frame buffer. The frame buffer shows low utilization. The heap fragments. Frame times spike unpredictably. The root cause—a library's 10% optimization—takes weeks to diagnose.

The 10% microbenchmark win cost weeks of debugging and months of degraded production performance.

Part IV explains the full design rationale for honoring the allocator contract.

---

## Part II — The Solutions

Part I showed that bypassing `allocator_traits::construct()` breaks the allocator protocol for PMR and custom allocators. The solution is simple: don't bypass it.

### Understanding allocator_traits::construct

The `std::allocator_traits` class template provides a uniform interface for working with allocators. Its `construct()` function is the canonical way to create objects in allocator-provided storage:

```cpp
template<class T, class Alloc>
struct allocator_traits {
    template<class U, class... Args>
    static void construct(Alloc& a, U* p, Args&&... args) {
        // If allocator has construct(), call it
        // Otherwise, use placement new
        if constexpr (has_construct_v<Alloc, U*, Args...>) {
            a.construct(p, std::forward<Args>(args)...);
        } else {
            ::new (static_cast<void*>(p)) U(std::forward<Args>(args)...);
        }
    }
};
```

This design allows allocators to customize construction behavior while providing a default for allocators that don't need customization. The key point is that the decision belongs to the allocator, not to the container.

### Why PMR Requires the Full Protocol

The PMR system's power comes from its `construct()` implementation. When you create a `std::pmr::vector<std::pmr::string>`, the vector's allocator automatically propagates to each string. This happens because `pmr::polymorphic_allocator::construct()` checks for `uses_allocator_v` and passes itself to types that accept allocators.

Consider what happens with a correctly implemented container:

```cpp
std::pmr::monotonic_buffer_resource pool;
std::pmr::vector<std::pmr::string> strings{&pool};

strings.emplace_back("hello");  // String uses pool, not heap
strings.emplace_back("world");  // String uses pool, not heap
```

Every string lives in the same memory pool as the vector. Deallocation is a single operation. Memory locality is preserved.

Now consider what happens with a container that bypasses `construct()`:

```cpp
std::pmr::monotonic_buffer_resource pool;
boost::container::small_vector<std::pmr::string, 4, 
    std::pmr::polymorphic_allocator<std::pmr::string>> strings{&pool};

strings.emplace_back("hello");  // String uses... which allocator?
```

If boost's small_vector bypasses `allocator_traits::construct()` for this operation, the string receives no allocator. It falls back to `std::pmr::get_default_resource()`, which is typically the global heap. The vector's data is in the pool; the strings' data is scattered across the heap.

### The Guarantees Table

| Guarantee | fat_p::SmallVector | boost::small_vector |
|-----------|-------------------|---------------------|
| Calls `allocator_traits::construct()` for all constructions | **Yes** | No (bypassed for trivial types) |
| PMR allocators propagate correctly | **Yes** | No |
| `scoped_allocator_adaptor` works correctly | **Yes** | No |
| Custom allocator instrumentation sees all constructions | **Yes** | No |
| Fast-path throughput (isolated benchmark) | 1.20 ns/op | 1.09 ns/op |
| Cache-warm fill/clear cycle | 0.61 ns/op | Not measured |
| Inline vs heap speedup | 5.7x-17.6x | Similar |

**Where fat_p loses:** In isolated fast-path microbenchmarks measuring only `emplace_back` throughput, fat_p is ~10% slower than boost.

**Where fat_p wins:** In every scenario involving PMR, `scoped_allocator_adaptor`, or custom allocators with instrumentation. Also in cache-warm reuse scenarios.

---

## Part III — The Investigation

### Context: The Optimization Journey

Part I introduced the situation: after optimizing SmallVector's fast path, we achieved parity with pre-reserved `std::vector` but discovered a 10% gap to boost. This section details the optimization work, the benchmark methodology, and how we traced the gap to its root cause.

The existing benchmark suite measured SmallVector against `std::vector` without `reserve()`. This comparison shows the allocation avoidance benefit (the 5.7x-17.6x win), but it conflates two different effects:

1. **Allocation avoidance** — SmallVector uses inline storage; `std::vector` calls malloc
2. **Fast-path efficiency** — How efficiently SmallVector executes `emplace_back` when no reallocation is needed

The existing benchmarks couldn't separate these effects. A 7x speedup might be 6.5x from allocation avoidance and 0.5x from fast-path efficiency—or it might be 7x from allocation avoidance with the fast path actually being *slower* than it should be.

### The Optimization Work

Before measuring, significant optimization work had already been done on SmallVector's `emplace_back` implementation. (For the complete list of optimizations considered, see [Appendix D](#appendix-d-full-optimization-history).)

**Optimization 1: Fast/Slow Path Split**

The original implementation checked for reallocation need on every `emplace_back`:

```cpp
// BEFORE: Every emplace_back pays for reallocation check
template<typename... Args>
reference emplace_back(Args&&... args) {
    if (mSize == capacity()) {
        growAndEmplace(std::forward<Args>(args)...);  // Slow path
    } else {
        AllocTraits::construct(mAllocator, end(), std::forward<Args>(args)...);
        ++mSize;
    }
    return back();
}
```

The optimization restructured this to make the common case (no reallocation) as streamlined as possible, with the growth logic factored into a separate non-inline function.

**Optimization 2: Memory Layout Reordering**

The class member layout was reordered to keep hot data (size, capacity, data pointer) in the same cache line, minimizing memory accesses on the fast path.

**Optimization 3: Redundant Load Elimination**

The fast path was audited to ensure no redundant loads of member variables. Each value needed for construction is loaded exactly once.

**Optimization 4: Branch Prediction Hints**

The reallocation check was annotated with `[[unlikely]]` to help the branch predictor optimize for the common case.

### The Measurement Problem

After these optimizations, the question remained: *how much did they help?* The existing benchmarks couldn't answer this because they compared against allocating `std::vector`. Any fast-path improvement would be swamped by the allocation overhead difference.

What was needed was a benchmark that isolates the fast path by comparing SmallVector against `std::vector` with `reserve()`—both operating on pre-allocated storage, both executing zero reallocations. This comparison would reveal the true cost of SmallVector's fast-path code versus the theoretical minimum.

### The New Benchmark

A new benchmark section was added specifically to measure fast-path throughput:

```cpp
// Measures pure fast-path performance, no allocations
void benchmark_fast_path_throughput() {
    constexpr size_t INLINE_CAP = 16;
    constexpr size_t N = 16;  // Exactly inline capacity
    constexpr size_t ITERATIONS = 100000;
    
    for (size_t iter = 0; iter < ITERATIONS; ++iter) {
        fat_p::SmallVector<int64_t, INLINE_CAP> vec;
        // No reserve - uses inline storage, guaranteed no heap
        for (size_t i = 0; i < N; ++i) {
            vec.emplace_back(static_cast<int64_t>(i));
        }
        benchmark_sink += vec.size();
    }
}
```

The key design decisions:

- **N = InlineCapacity** — Guarantees 100% fast-path execution, zero reallocations
- **Compare against `std::vector` with `reserve(N)`** — Both containers operate on pre-allocated storage
- **Interleaved execution** — Reduces systematic bias from cache state or thermal throttling
- **Multiple inline capacities** — Tests whether the pattern holds across different sizes

This benchmark answers the question: "When both containers have pre-allocated storage, how does SmallVector's `emplace_back` compare to `std::vector`'s?"

### The Results and the Gap

The benchmark revealed two things:

**Good news:** The optimizations worked. SmallVector's fast path achieved parity with pre-reserved `std::vector`:

```
InlineCap | SmallVector (ns/op) | std::vector* (ns/op) | Ratio
----------|---------------------|----------------------|------
        4 |                3.34 |                 3.35 |  1.00x
        8 |                1.78 |                 1.76 |  0.99x
       16 |                1.04 |                 1.03 |  0.98x
       32 |                0.86 |                 0.88 |  1.02x
```

Parity with `std::vector` on pre-reserved storage is the theoretical best case for an allocator-compliant container. The optimizations achieved their goal.

**The question:** But how does this compare to other SmallVector implementations? Adding boost::small_vector to the comparison revealed an unexpected gap:

```
Library       | emplace_back (ns/op)
--------------|---------------------
fat_p::SV    |                1.20
boost::sv    |                1.09
std::vector* |                1.01
```

fat_p was at parity with `std::vector`, but boost was ~10% faster than both. Where was the extra performance coming from?

### Hypothesis: Where Does the Gap Come From?

With all other optimizations applied, the remaining difference had to come from the construction path itself. The hypothesis: boost bypasses `allocator_traits::construct()` for trivially copyable types.

To test this, we examined what different construction paths generate. The critical difference is whether the allocator's `construct()` method is called:

```cpp
// Path A: Full allocator protocol (fat_p)
AllocTraits::construct(mAllocator, slot, std::forward<Args>(args)...);
// Compiles to: call through allocator_traits, then placement new

// Path B: Direct placement new (hypothesized boost path)
::new (static_cast<void*>(slot)) T(std::forward<Args>(args)...);
// Compiles to: direct placement new, no allocator involvement
```

For `std::allocator<T>`, both paths ultimately execute placement new. But Path A goes through an additional layer of template instantiation and (potentially) an indirect call. Even with inlining, the optimizer may generate slightly different code.

### Verification: What Boost Actually Does

Arthur O'Dwyer's article "A not-so-quick introduction to the C++ allocator model" [1] documents this exact issue. The boost::container library optimizes for `std::allocator` by bypassing `allocator_traits::construct()` when it determines the type is trivially constructible.

The optimization is documented in boost's design notes as a performance decision. The tradeoff is explicit: faster microbenchmarks in exchange for reduced allocator generality.

This verification confirms the hypothesis. The 10% gap is not a fixable inefficiency in fat_p—it's the cost of honoring the allocator contract.

---

## Part IV — Foundations

### Design Rationale

fat_p::SmallVector honors the allocator contract for three reasons:

**1. Correctness over benchmarks**

A container that silently breaks PMR is not a valid C++ container. It may pass unit tests (which typically use `std::allocator`) while failing in production (which may use PMR or custom allocators). Silent failures are worse than slow code.

**2. The 10% doesn't matter in practice**

SmallVector's value proposition is avoiding heap allocation for small collections. The benchmark comparison that matters is SmallVector vs `std::vector` without reserve—and there, SmallVector wins by 5.7x to 17.6x. The 10% gap to boost only shows up when comparing against pre-reserved storage, which defeats the purpose of using SmallVector.

| Benchmark Scenario | fat_p vs std::vector | fat_p vs boost |
|-------------------|---------------------|----------------|
| Inline storage (N ≤ capacity) | 5.7x-17.6x faster | ~10% slower |
| Cache-warm reuse | 10% faster | N/A |
| Real workloads with allocation | Dominant win | Negligible difference |

When allocation is in play, the 10% construct overhead is noise compared to the malloc/free cost saved.

**3. Library reputation is fragile**

When a user discovers their PMR-based memory architecture is being silently violated by a "small_vector" implementation, they lose trust in the entire library. That trust is not recoverable. The 10% microbenchmark win is not worth the support tickets, bug reports, and reputation damage.

### Rejected Alternatives

**Alternative A: Bypass for std::allocator only**

```cpp
if constexpr (std::is_same_v<Allocator, std::allocator<T>>) {
    ::new (slot) T(std::forward<Args>(args)...);
} else {
    AllocTraits::construct(mAllocator, slot, std::forward<Args>(args)...);
}
```

This would recover the 10% for the common case while preserving correctness for custom allocators.

**Rejected because:** It creates a behavioral difference between allocator types. Code that works with `std::allocator` might silently break when switched to a custom allocator. This is exactly the kind of "works in test, fails in production" bug we're trying to avoid.

**Alternative B: Document the limitation**

Accept the bypass, document that PMR and custom allocators may not work correctly, and let users decide.

**Rejected because:** Users don't read documentation until something breaks. By then, they've spent hours debugging. The documentation approach shifts the burden to users for a library design decision.

**Alternative C: Provide a policy**

Add a template parameter that controls whether to bypass `construct()`:

```cpp
template<typename T, size_t N, typename Alloc, 
         bool BypassConstruct = false>
class SmallVector;
```

**Rejected because:** Complexity explosion. Every SmallVector user would need to understand the tradeoff and make a decision. Most would pick the default (for good or ill). The correct default is "honor the contract."

### The Broader Lesson: Optimizing for the Right Metric

The `construct()` bypass illustrates a pattern worth examining: optimizing for a benchmark that doesn't reflect the container's purpose. The 10% gap only appears when comparing fast-path throughput against boost—a benchmark that measures SmallVector operating on pre-allocated storage. But SmallVector's entire value proposition is *avoiding allocation*. If you're comparing pre-allocated performance, you've already conceded the use case that justifies SmallVector's existence.

This pattern—chasing microbenchmark wins that don't reflect real usage—appeared in another optimization we considered and rejected.

**The memcpy reallocation optimization**

When SmallVector needs to grow and the element type is trivially copyable, we could use `memcpy` instead of element-by-element move construction:

```cpp
if constexpr (std::is_trivially_copyable_v<T>) {
    std::memcpy(newStorage, oldStorage, mSize * sizeof(T));
} else {
    for (size_t i = 0; i < mSize; ++i) {
        AllocTraits::construct(mAllocator, newStorage + i, 
                               std::move(oldStorage[i]));
        AllocTraits::destroy(mAllocator, oldStorage + i);
    }
}
```

Unlike the `construct()` bypass, this optimization is *valid*—`memcpy` for trivially copyable types is well-defined and doesn't violate any contracts. A benchmark would show measurable improvement for reallocation-heavy workloads.

**Why we rejected it:** It optimizes for the wrong scenario.

SmallVector's value proposition is *avoiding reallocation entirely* by keeping small collections in inline storage. If you're hitting the reallocation path frequently, you've chosen the wrong inline capacity—or you're using the wrong container.

| Usage Pattern | Reallocation Frequency | memcpy Benefit |
|---------------|------------------------|----------------|
| N ≤ InlineCapacity (correct usage) | Never | Zero |
| N slightly > InlineCapacity (edge case) | Once | Negligible |
| N >> InlineCapacity (wrong container) | Multiple | Measurable but irrelevant |

If someone is pushing 10,000 elements into a `SmallVector<T, 16>`, the problem isn't that reallocation is slow—the problem is they should be using `std::vector` with `reserve()`. Optimizing the reallocation path encourages misuse of the container.

**The common thread**

Both the `construct()` bypass and the `memcpy` reallocation share a failure mode: they optimize for benchmarks that don't represent the container's intended use. The `construct()` bypass optimizes for pre-allocated throughput (irrelevant when allocation avoidance is the goal). The `memcpy` optimization optimizes for reallocation performance (irrelevant when avoiding reallocation is the goal).

Before optimizing, ask: *does this benchmark measure the use case that justifies this container's existence?* If not, the optimization—however valid—is solving the wrong problem.

For a systematic approach to identifying and validating the right performance metrics, see *Handbook - Performance Engineering Methodology* [4].

### The Real Performance Story

The benchmark that matters for SmallVector is not "fast-path throughput vs boost." It's "SmallVector vs std::vector in real code." Here's what the full benchmark suite shows:

```
================================================================================
  INLINE VS HEAP (The Real Win)
================================================================================

Size | SmallVector push_back (ns) | std::vector push_back (ns) | Ratio
   4 |                       3.96 |                      22.64 |  5.71x
   8 |                       2.34 |                      16.35 |  6.99x
  12 |                       1.33 |                      19.58 | 14.70x
  16 |                       1.09 |                      19.07 | 17.56x

================================================================================
  CACHE-WARM REUSE (Secondary Win)
================================================================================

SmallVector (reused): 0.61 ns/op
std::vector (reused): 0.68 ns/op
Ratio: 1.11x (SmallVector faster)
```

When you're getting 17.56x improvement over `std::vector`, quibbling about 10% vs boost is missing the forest for the trees.

---

## Design Rules to Internalize

1. **The allocator contract is non-negotiable.** `allocator_traits::construct()` must be called for all object construction in allocator-aware containers. No exceptions for "trivial" types.

2. **Microbenchmarks lie by omission.** A benchmark that shows 10% improvement may be hiding silent correctness bugs that only manifest with non-default allocators.

3. **PMR is not optional.** C++17 introduced PMR as a first-class feature. Containers that break PMR are not valid C++17 containers, regardless of what the benchmarks say.

4. **Test with custom allocators.** If your unit tests only use `std::allocator`, you're not testing allocator compliance. Add tests with PMR and instrumented allocators.

5. **Optimize the right thing.** SmallVector's value is avoiding allocation, not being 10% faster than boost at construction. Optimize for the use case that matters.

6. **Silent failures are unacceptable.** Code that "works" but silently uses the wrong allocator is worse than code that fails loudly. The PMR violation produces no error, no warning, no indication that anything is wrong—until production memory architecture fails.

7. **Library reputation is built on correctness.** Users forgive slowness. They don't forgive "your library corrupted my memory tracking and I spent a week debugging it."

---

## What To Do Now

**If you're evaluating SmallVector implementations:**

1. Test with `std::pmr::polymorphic_allocator`. Create a `SmallVector<SmallVector<int>>` with PMR. Verify the inner vectors use the same memory resource as the outer vector.

2. Test with an instrumented allocator that counts `construct()` calls. Verify the count matches the number of elements inserted.

3. If a library fails these tests, understand what you're giving up before using it.

**If you're implementing a container:**

1. Always use `std::allocator_traits<Alloc>::construct()`. Never use placement new directly for element construction.

2. Never add `if constexpr (is_trivially_copyable_v<T>)` optimizations that bypass the allocator.

3. Add PMR tests to your test suite. They'll catch allocator contract violations.

**If you're using boost::small_vector with PMR:**

1. Be aware that nested containers may not receive the propagated allocator.

2. Consider whether the 10% microbenchmark win is worth the allocator compliance loss for your use case.

3. If you need full PMR support, use fat_p::SmallVector or implement your own compliant container.

**If you're chasing benchmark numbers:**

1. Ask yourself what the benchmark is actually measuring. Is it the use case you care about?

2. A 10% win in isolated throughput may be meaningless if the real bottleneck is allocation (where SmallVector wins by 17x).

3. Never optimize by violating contracts. The benchmark win evaporates when the silent bug manifests.

---

## Appendix A: Benchmark Methodology

The benchmarks in this case study were run on Windows/MSVC with the following configuration:

- CPU affinity set to non-zero core (avoid OS scheduler interference)
- 3 warmup iterations, 5 measured iterations per test
- Results reported as mean with standard deviation
- Compiler: MSVC (cl.exe) with `/O2 /DNDEBUG`

The fast-path throughput benchmark specifically measures:
- InlineCapacity = N
- Exactly N elements pushed (100% inline storage, 0% heap)
- 100,000 iterations per measurement
- Interleaved SmallVector/std::vector execution to reduce systematic bias

For comprehensive guidance on designing benchmarks that measure the right things, avoiding common pitfalls, and interpreting results correctly, see *Handbook - Performance Engineering Methodology* [4].

## Appendix B: The PMR Test

To verify allocator propagation, use this test pattern:

```cpp
#include <memory_resource>
#include <cassert>
#include "SmallVector.h"

void test_pmr_propagation() {
    // Track allocations
    std::pmr::monotonic_buffer_resource pool;
    std::pmr::polymorphic_allocator<int> alloc{&pool};
    
    // Outer vector with PMR
    using InnerVec = fat_p::SmallVector<int, 4, 
        std::pmr::polymorphic_allocator<int>>;
    fat_p::SmallVector<InnerVec, 2, 
        std::pmr::polymorphic_allocator<InnerVec>> outer{alloc};
    
    // Add inner vector - should receive propagated allocator
    outer.emplace_back();
    outer[0].push_back(42);
    
    // Verify inner vector uses same allocator
    assert(outer[0].get_allocator().resource() == &pool);
    // If this fails, construct() was bypassed
}
```

If this test fails with a SmallVector implementation, that implementation bypasses `allocator_traits::construct()` and is not PMR-compliant.

## Appendix C: Why PMR Exists

### The Problem PMR Solves

Before C++17, using custom allocators in C++ was painful. Every container type with a different allocator was a *different type*:

```cpp
std::vector<int, std::allocator<int>> v1;
std::vector<int, MyPoolAllocator<int>> v2;

// These are DIFFERENT TYPES - cannot assign, cannot pass to same function
v1 = v2;  // Compile error
```

This meant that functions accepting containers had to be templates, or you had to choose one allocator at API boundaries and stick with it. Mixing allocators was nearly impossible without templates everywhere.

The deeper problem was *allocator propagation*. When you have nested containers—a vector of vectors, a map of strings—each nested container needs its own allocator. Before C++17, propagating allocators to nested containers required explicit, error-prone code at every construction site.

### What PMR Provides

PMR (Polymorphic Memory Resources) solves both problems through runtime polymorphism:

**1. Type erasure:** All PMR containers share the same type regardless of which memory resource they use:

```cpp
std::pmr::vector<int> v1;  // Uses default resource
std::pmr::vector<int> v2;  // Uses same type!

std::pmr::monotonic_buffer_resource pool;
std::pmr::vector<int> v3{&pool};  // Still same type

v1 = v3;  // Works - same type
void process(std::pmr::vector<int>& v);  // No template needed
```

**2. Automatic propagation:** The `polymorphic_allocator::construct()` method automatically passes the allocator to nested containers that support it:

```cpp
std::pmr::monotonic_buffer_resource pool;
std::pmr::vector<std::pmr::string> strings{&pool};

strings.emplace_back("hello");
// The string AUTOMATICALLY uses the same pool as the vector
// No explicit allocator passing required
```

This propagation is the key feature that the `construct()` bypass breaks.

### Real-World PMR Use Cases

**Game engines:** Per-frame allocators that bulk-deallocate at frame end. All temporary allocations during a frame use a monotonic buffer, avoiding thousands of individual `free()` calls.

**High-frequency trading:** Memory pools pre-allocated at startup to avoid allocation latency during trading. All order objects, market data, and temporary collections use the pre-allocated pools.

**Embedded systems:** Fixed memory budgets where heap fragmentation is unacceptable. PMR allows using stack buffers or static arrays as backing storage.

**Server applications:** Per-request memory isolation where each request gets its own arena, preventing memory leaks from accumulating across requests.

In all these cases, the value of PMR depends on *all* allocations going through the designated resource. A container that silently bypasses `construct()` and allocates from the global heap defeats the entire architecture.

### The Contract

PMR's automatic propagation works because containers call `allocator_traits::construct()`, and `polymorphic_allocator::construct()` implements the uses-allocator protocol. This is a contract:

- **Container's obligation:** Call `allocator_traits::construct()` for all element construction
- **Allocator's obligation:** Propagate itself to nested allocator-aware types in `construct()`
- **Result:** Nested containers automatically share the memory resource

Breaking the container's side of this contract—by bypassing `construct()`—silently breaks PMR. The nested containers fall back to `std::pmr::get_default_resource()`, which is typically the global heap. No error, no warning, just silent architectural violation.

## Appendix D: The Full Optimization History

The investigation documented in this case study was part of a broader SmallVector optimization effort. This appendix provides the full context of optimizations considered, implemented, and rejected.

### The Original Benchmark Picture

Initial benchmarks showed SmallVector achieving its primary goal:

| Size | SmallVector | std::vector | Speedup |
|------|-------------|-------------|---------|
| 4 (inline) | 10.7 ns | 85.9 ns | **8x** |
| 8 (inline) | 8.0 ns | 66.1 ns | **8x** |
| 16 (inline) | 6.1 ns | 44.9 ns | **7x** |

But gaps were also visible in pre-reserved scenarios:

| Operation (N=10000, pre-reserved) | SmallVector | std::vector | boost |
|-----------------------------------|-------------|-------------|-------|
| push_back | 5.4 ns | 3.4 ns | 1.8 ns |
| emplace_back | 5.4 ns | 1.8 ns | 1.8 ns |

### Optimizations Implemented

**1. Fast/Slow Path Split**

The entire `emplace_back` function (~70 lines including reallocation logic) was split into:
- `emplace_back()` — ~15 lines, `FATP_FORCEINLINE`, handles the common case
- `emplace_back_slow()` — `FATP_NOINLINE`, handles reallocation

Rationale: Fast path compiles to ~60 bytes vs ~400 bytes combined. Better I-cache utilization in tight loops.

**2. Data Member Layout Reorder**

Hot fields (`mData`, `mSize`, `mCapacity`) moved to the front of the class, ensuring they share the first cache line:

```cpp
// BEFORE: inline buffer first, hot fields at offset 128+
alignas(T) std::byte mInlineBuffer[InlineCapacity * sizeof(T)];
T* mData;
size_t mSize;
size_t mCapacity;

// AFTER: hot fields first
T* mData;           // offset 0
size_t mSize;       // offset 8
size_t mCapacity;   // offset 16
Allocator mAllocator;
alignas(T) std::byte mInlineBuffer[...];
```

**3. Cache Slot Pointer Before Increment**

```cpp
// BEFORE: redundant address computation
AllocTraits::construct(mAllocator, mData + mSize, ...);
++mSize;
return mData[mSize - 1];  // recomputes mData + mSize - 1

// AFTER: reuse computed pointer
T* slot = mData + mSize;
AllocTraits::construct(mAllocator, slot, ...);
++mSize;
return *slot;
```

**4. Remove `assert_invariants()` from Fast Path**

Invariant checking moved to slow path only (after reallocation), eliminating function call overhead from the hot path.

### Optimizations Rejected

**5. memcpy for Trivially Copyable Types**

Proposed using `memcpy` for bulk operations during reallocation and copy construction.

Rejected because: Bypasses `AllocTraits::construct()`, violating the allocator contract. Also optimizes for large-N scenarios that represent misuse of SmallVector.

**6. Direct Placement new Instead of AllocTraits::construct**

Proposed bypassing `AllocTraits::construct()` for `std::allocator<T>`.

Initially dismissed with the rationale: "modern compilers inline `AllocTraits::construct` for `std::allocator` anyway, so the benefit is marginal."

This assessment proved incomplete. The fast-path isolation benchmark (created later) revealed a measurable 10% gap to boost. The deeper investigation documented in this case study confirmed:
- The gap is real (~10%)
- It comes from boost bypassing the allocator contract
- The original decision to reject the optimization was correct, but for more important reasons than initially understood

**7. Optimizing Large-N Pre-Reserved Performance**

Dismissed as irrelevant: "If someone calls `reserve(10000)` on a SmallVector, they're using the wrong container."

### The Missing Benchmark

The original benchmarks couldn't separate allocation avoidance from fast-path efficiency. The "Inline vs Heap" benchmark showed 7-8x wins, but this conflated:
1. Allocation avoidance (SmallVector uses inline storage; `std::vector` calls malloc)
2. Fast-path code efficiency (how well the `emplace_back` hot path is optimized)

The fast-path isolation benchmark described in Part III was created specifically to answer: "After implementing optimizations #1-4, how does our fast path compare to the theoretical minimum?"

The answer: parity with pre-reserved `std::vector`, but 10% slower than boost—leading to the allocator contract investigation.

### Decision Matrix (Original Report)

| Optimization | Implemented | Risk | Rationale |
|--------------|-------------|------|-----------|
| Fast/slow split | ✅ Yes | None | Pure refactor, better I-cache |
| Layout reorder | ✅ Yes | None | Zero-cost cache optimization |
| Cache slot pointer | ✅ Yes | None | Free micro-optimization |
| Remove assert from fast path | ✅ Yes | None | No function call overhead |
| memcpy for trivial types | ❌ No | Medium | Violates allocator contract |
| Direct placement new | ❌ No | Medium | Violates allocator contract |
| Optimize large N performance | ❌ No | N/A | Wrong use case for SmallVector |

The implemented optimizations achieved an estimated 15-25% improvement on tight loops within inline capacity, with zero behavioral changes and full allocator contract compliance.

## Appendix D: Full Optimization History

This case study focused on one decision—the `allocator_traits::construct()` bypass. But that decision emerged from a broader optimization effort. This appendix documents the full history for completeness.

### The Optimization Pass

After SmallVector achieved its primary goal (7-8x speedup for inline storage vs `std::vector`), a systematic optimization pass examined all potential improvements. Eight optimizations were considered:

| # | Optimization | Decision | Rationale |
|---|--------------|----------|-----------|
| 1 | Fast/slow path split for `emplace_back` | ✅ Implemented | Pure refactor, better I-cache utilization |
| 2 | Data member layout reorder | ✅ Implemented | Hot fields in first cache line |
| 3 | Cache slot pointer before increment | ✅ Implemented | Saves 1-2 instructions per call |
| 4 | Remove `assert_invariants()` from fast path | ✅ Implemented | No function call overhead in hot path |
| 5 | `memcpy` for trivially copyable types | ❌ Rejected | Violates allocator contract |
| 6 | Direct placement `new` instead of `AllocTraits::construct` | ❌ Rejected | Violates allocator contract |
| 7 | Optimize pre-reserved heap performance | ❌ Dismissed | Wrong use case for SmallVector |
| 8 | Optimize copy/move for large N | ❌ Dismissed | Wrong use case for SmallVector |

### What Was Implemented

**Optimization 1: Fast/Slow Path Split**

The entire `emplace_back` function (~70 lines including reallocation logic) was split into:
- `emplace_back()` — ~15 lines, `FATP_FORCEINLINE`, handles the common case
- `emplace_back_slow()` — `FATP_NOINLINE`, handles reallocation

The fast path compiles to ~60 bytes vs ~400 bytes combined, improving I-cache utilization in tight loops.

**Optimization 2: Data Member Layout Reorder**

Hot fields were moved to the front of the class:

```cpp
// BEFORE: inline_buffer_ first
alignas(T) std::byte inline_buffer_[InlineCapacity * sizeof(T)];  // offset 0
T* mData;           // offset 128 — SECOND cache line
size_t mSize;       // offset 136
size_t mCapacity;   // offset 144

// AFTER: hot fields first  
T* mData;           // offset 0  — FIRST cache line
size_t mSize;       // offset 8
size_t mCapacity;   // offset 16
Allocator mAllocator;  // offset 24 (often zero-size via EBO)
alignas(T) std::byte inline_buffer_[...];  // offset 24+ (cold)
```

**Optimization 3: Cache Slot Pointer**

```cpp
// BEFORE
AllocTraits::construct(mAllocator, mData + mSize, ...);
++mSize;
return mData[mSize - 1];  // Redundant recomputation

// AFTER
T* slot = mData + mSize;
AllocTraits::construct(mAllocator, slot, ...);
++mSize;
return *slot;  // Reuse computed pointer
```

**Optimization 4: Remove Invariant Check from Fast Path**

`assert_invariants()` was moved to the slow path only, eliminating function call overhead from the hot path.

### What Was Rejected

**Optimization 5: memcpy for Trivially Copyable Types**

```cpp
// REJECTED
if constexpr (std::is_trivially_copyable_v<T>) {
    std::memcpy(new_data, mData, mSize * sizeof(T));
} else {
    // element-wise copy/move
}
```

Rejected for two reasons:
1. Bypasses `AllocTraits::construct()`, violating the allocator contract
2. Optimizes for large N copies—a use case that contradicts SmallVector's purpose

**Optimization 6: Direct Placement new**

```cpp
// REJECTED
if constexpr (std::is_same_v<Allocator, std::allocator<T>>) {
    ::new (static_cast<void*>(slot)) T(std::forward<Args>(args)...);
} else {
    AllocTraits::construct(mAllocator, slot, std::forward<Args>(args)...);
}
```

The original rejection noted: "modern compilers inline `AllocTraits::construct` for `std::allocator` anyway, so the benefit is marginal."

This turned out to be incorrect. The fast-path benchmark (created later) revealed a measurable 10% gap. The decision was right, but the reasoning was incomplete—the real reason is allocator contract compliance, not "marginal benefit."

**Optimizations 7-8: Large N Performance**

Benchmarks showed SmallVector was 2-3x slower than boost for pre-reserved N=10,000. These were dismissed as irrelevant:

> "If someone calls `reserve(10000)` on a SmallVector, they're using the wrong container."

### The Sequence of Discovery

1. **Initial optimization pass** — Implemented #1-4, rejected #5-6 with incomplete reasoning
2. **Benchmark gap observed** — After optimizations, noticed boost was still faster in some scenarios  
3. **New benchmark created** — Fast-path throughput benchmark to isolate the effect
4. **10% gap confirmed** — SmallVector at parity with `std::vector`, but boost 10% faster
5. **Root cause identified** — boost bypasses `allocator_traits::construct()`
6. **Decision validated** — The 10% cost is the price of allocator contract compliance

The case study narrative compresses this into a linear investigation, but the actual path involved discovering that an earlier decision was right for deeper reasons than originally understood.

---

## References

1. **O'Dwyer, Arthur.** "A not-so-quick introduction to the C++ allocator model." *quuxplusone.github.io*, 2018. https://quuxplusone.github.io/blog/2018/06/01/what-is-the-allocator-model/

2. **ISO/IEC 14882:2017.** *Programming Languages — C++.* Section [allocator.uses.construction] — Uses-allocator construction.

3. **Boost.Container documentation.** "small_vector" and allocator handling. https://www.boost.org/doc/libs/release/doc/html/container.html

4. **FAT-P Documentation.** *Handbook - Performance Engineering Methodology.* Guidelines for benchmark design and interpretation.

5. **FAT-P Documentation.** *Case Study - SmallVector.* Analysis of allocation avoidance and memory hierarchy effects.

---

*Case Study - The Allocator Contract and the 10% Trap — January 2026*
