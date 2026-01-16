---
doc_id: CS-SMALLVECTOR-002
doc_type: "Case Study"
title: "The Allocator Contract and the 10% Trap"
fatp_components: ["SmallVector"]
topics: ["allocator_traits::construct", "allocator hook observability", "microbenchmark trap", "benchmark-driven development", "PMR", "memcpy relocation"]
constraints: ["allocator contract", "polymorphic memory resources", "custom allocator instrumentation", "uses-allocator construction"]
cxx_standard: "C++17"
std_equivalent: "std::inplace_vector"
std_since: "C++26"
boost_equivalent: "Boost.Container small_vector"
build_modes: ["Release"]
last_verified: "2026-01-15"
audience: ["C++ developers", "library maintainers", "performance engineers", "AI assistants"]
status: "draft"
---

# Case Study - The Allocator Contract and the 10% Trap
## Why chasing benchmark parity leads to semantic purchases, and how to recognize when you're optimizing the wrong metric

**Scope:** This case study examines the temptation to "close the gap" in microbenchmarks by bypassing `std::allocator_traits<Alloc>::construct()`. We show why this optimization is a semantic purchase that breaks allocator observability, and why the benchmark pressure itself indicates you're measuring the wrong thing.

**Not covered:**
- SmallVector's inline storage mechanism (see *Case Study - SmallVector*)
- General allocator design patterns
- Memory pool implementations
- Boost's internal design decisions

**Prerequisites:**
- C++17 proficiency (allocator model, `std::allocator_traits`)
- Familiarity with custom allocators and their use cases
- Understanding of placement new vs allocator-aware construction
- Basic awareness of benchmark methodology

## Case Study Card

**Problem:** After optimizing SmallVector's fast path to parity with `std::vector`, pre-reserved throughput benchmarks still showed a ~10-17% gap; comparing against Boost.Container `small_vector` created pressure to "close the gap"  
**Constraint:** The C++ allocator model requires containers to route construction through `allocator_traits::construct()`, preserving allocator hooks  
**Symptom:** Benchmark pressure to bypass allocator-aware construction or add memtransfer optimization  
**Root cause:** Optimizing for a benchmark that doesn't measure the container's value proposition  
**Fix pattern:** Recognize the benchmark trap; reject semantic purchases; optimize only for the metric that matters (allocation avoidance)  
**FAT-P components used:** SmallVector  
**Build-mode gotchas:** None (same semantics in Debug and Release)  
**Guarantees:** Full allocator contract compliance; allocator construction hooks remain observable  
**Non-guarantees:** Does not guarantee parity in benchmarks that remove allocation from the measurement; does not include Boost's memtransfer optimization

## Table of Contents

- [⚠️ Before You Read Further: The Benchmark That Lies](#before-you-read-further-the-benchmark-that-lies)
- [Part I — The Problems](#part-i--the-problems)
- [Part II — The Solutions](#part-ii--the-solutions)
- [Part III — The Investigation](#part-iii--the-investigation)
- [Part IV — Foundations](#part-iv--foundations)
- [Design Rules to Internalize](#design-rules-to-internalize)
- [What To Do Now](#what-to-do-now)
- [Appendix A: The Instrumented Allocator Test](#appendix-a-the-instrumented-allocator-test)
- [Appendix B: Why PMR Exists](#appendix-b-why-pmr-exists)
- [Appendix C: The Full Optimization History](#appendix-c-the-full-optimization-history)
- [References](#references)

---

## ⚠️ Before You Read Further: The Benchmark That Lies

You've built a SmallVector. Your inline-vs-heap benchmarks show 5-10x wins over `std::vector`:

```
Size | SmallVector (ns/op) | std::vector (ns/op) | Speedup
-----|---------------------|---------------------|--------
   4 |                4.34 |               22.80 |   5.25x
   8 |                2.31 |               16.66 |   7.23x
  16 |                1.16 |               11.17 |   9.62x
```

Success. But then you run a different benchmark—pre-reserved throughput at large N:

```
Operation (N=10000, pre-reserved) | SmallVector | std::vector | Gap
----------------------------------|-------------|-------------|-----
push_back                         |    0.84 ns  |    0.72 ns  | 17%
```

**17% slower.** The temptation is immediate: close the gap. The obvious fix:

```cpp
// THE TRAP: Bypass allocator_traits for "trivially copyable" types
template<typename... Args>
void emplace_back(Args&&... args) {
    if constexpr (std::is_trivially_copyable_v<T>) {
        ::new (static_cast<void*>(slot)) T(std::forward<Args>(args)...);  // Faster!
    } else {
        AllocTraits::construct(mAllocator, slot, std::forward<Args>(args)...);
    }
}
```

**Stop.** This benchmark is lying to you.

The benchmark measures pre-reserved throughput—a scenario where SmallVector provides *zero value*. If you're calling `reserve(10000)` on a SmallVector, you're using the wrong container. The 17% gap exists in exactly the scenario where SmallVector shouldn't be used.

Meanwhile, the "fix" breaks:
- **Instrumented allocators** — construction counters become wrong
- **PMR allocators** — nested container propagation fails silently
- **Debug allocators** — poisoning/tracking hooks are bypassed

The benchmark pressure is real. The optimization is a trap. Part I explains why.

---

## Part I — The Problems

### The Obvious Approach

SmallVector exists to solve one problem: avoid heap allocation for small, temporary collections. The benchmarks prove it works—5-10x faster than `std::vector` when data fits in inline storage. After achieving that primary goal, a natural question arises: *is the implementation as efficient as it could be?*

To answer this, you build a new benchmark. You compare SmallVector against `std::vector` with `reserve()`, both operating on pre-allocated storage. This isolates the fast-path code from allocation effects. The results show parity with `std::vector`—the theoretical best case for an allocator-compliant container.

But then you run at larger N. And a gap appears:

| N | SmallVector | std::vector | Gap |
|---|-------------|-------------|-----|
| 100 | 1.00 ns/op | 1.00 ns/op | 0% |
| 1,000 | 0.60 ns/op | 0.60 ns/op | 0% |
| 10,000 | 0.84 ns/op | 0.72 ns/op | 17% |

The obvious response: find the bottleneck and fix it. The construction path is the prime suspect. If you bypass `allocator_traits::construct()` and use direct placement new, the gap might close.

### The Hidden Constraint

The C++ allocator model isn't just about memory allocation. It's a protocol with three distinct operations:

1. **`allocate()`** — obtain raw memory
2. **`construct()`** — create an object in that memory
3. **`destroy()`** + **`deallocate()`** — tear down and release

The critical insight: `construct()` is not equivalent to placement new for all allocators. For `std::allocator<T>`, they're identical. But custom allocators use `construct()` for:

- **Counting:** Track how many objects were constructed for leak detection
- **Poisoning:** Fill memory with sentinel values before/after construction
- **Logging:** Record construction events for debugging
- **Propagation:** Pass the allocator to nested allocator-aware types (PMR)

When a container bypasses `allocator_traits::construct()`, these hooks become invisible. The allocator's view of the program no longer matches reality.

### The Symptoms

The symptoms of bypassing `construct()` are insidious because they're silent:

**Corrupted allocation tracking:** Your custom allocator counts constructions and destructions. After switching to a container that bypasses `construct()`, the counts don't match. You spend days debugging your allocator before realizing the container never called `construct()`.

**Silent PMR mismatch:** Your outer container uses a memory pool. Your inner containers silently use the default heap. Everything "works" until the pool fills while the heap is nearly empty.

**Broken debug infrastructure:** Your debug allocator poisons memory after construction. The container bypasses `construct()`, so the poisoning never happens. Use-after-free bugs go undetected.

**Test passes, production fails:** Unit tests use `std::allocator` (where bypass is correct). Production uses instrumented allocators. The bug only manifests under production conditions.

### The Cost

The cost is difficult to quantify because failures are silent and delayed. Consider this scenario:

A team uses PMR to isolate per-request allocations. Each request gets a monotonic buffer that's bulk-deallocated at request end. They adopt a SmallVector implementation that bypasses `construct()`.

For months, everything works. Then someone adds `SmallVector<SmallVector<int>>` to a hot path. The outer SmallVector bypasses `construct()` for the inner SmallVectors. The inner SmallVectors silently use the default resource (global heap) instead of the request buffer.

The request buffer shows low utilization. The heap fragments. Latency spikes unpredictably. The root cause—a library's 10% optimization—takes weeks to diagnose.

### Solution Preview

The solution is simple: don't bypass `allocator_traits::construct()`. Ever.

But the deeper solution is recognizing *why the benchmark pressure exists*. The 17% gap appears in pre-reserved throughput—a benchmark that measures SmallVector in exactly the scenario where it provides no value. Optimizing for this benchmark is optimizing for the wrong metric.

Part IV explains the full design rationale.

---

## Part II — The Solutions

Part I showed that bypassing `allocator_traits::construct()` breaks allocator observability. The solution is straightforward: always route construction through `allocator_traits::construct()`.

### The Mechanism

The `std::allocator_traits` class template provides a uniform interface for allocator operations. Its `construct()` function is the canonical way to create objects in allocator-provided storage:

```cpp
template<class T, class Alloc>
struct allocator_traits {
    template<class U, class... Args>
    static void construct(Alloc& a, U* p, Args&&... args) {
        if constexpr (/* allocator has construct() */) {
            a.construct(p, std::forward<Args>(args)...);
        } else {
            ::new (static_cast<void*>(p)) U(std::forward<Args>(args)...);
        }
    }
};
```

The design allows allocators to customize construction while providing a default for allocators that don't need it. The key principle: **the decision belongs to the allocator, not to the container**.

A container that bypasses this mechanism is making a unilateral decision that the allocator's `construct()` hook doesn't matter. That decision is wrong for any allocator that uses the hook.

### Guarantees / Non-Guarantees

| Property | Guaranteed? | Conditions | Notes |
|----------|-------------|------------|-------|
| `allocator_traits::construct()` called for all constructions | ✅ Yes | All element types | Container never bypasses |
| PMR allocators propagate correctly | ✅ Yes | Allocator-aware element types | Via uses-allocator construction |
| Instrumented allocators see all constructions | ✅ Yes | Any allocator with `construct()` | No silent bypass |
| Parity with pre-reserved `std::vector` in throughput | ❌ No | Large N | May trail by ~10-17% |
| Parity in allocation-avoidance benchmarks | ✅ Yes | N ≤ InlineCapacity | This is the metric that matters |
| Memtransfer optimization in bulk operations | ❌ No | By design | Boost does this; we don't |

### Decision Guide

**When to use SmallVector:**
- Small, bounded collections (typically N ≤ 16)
- Temporary containers in hot loops
- When allocation overhead dominates

**When to use `std::vector` with `reserve()`:**
- Large collections (N >> InlineCapacity)
- When you know the size upfront
- When pre-reserved throughput matters

**When to investigate further:**
- If your SmallVector benchmarks show allocation overhead (you're measuring wrong)
- If your `std::vector` benchmarks don't show allocation overhead (you're measuring wrong)

### Where It Loses

SmallVector loses in two scenarios:

1. **Pre-reserved throughput at large N:** When comparing against `std::vector` with `reserve()`, SmallVector may trail by ~10-17%. This is acceptable because pre-reserved benchmarks remove allocation—the reason to use SmallVector.

2. **Reallocation-heavy workloads against Boost:** Boost.Container uses `memmove` for trivially copyable types during reallocation. In benchmarks that stress reallocation, Boost will be faster. This is acceptable because frequent reallocation indicates wrong container choice or wrong inline capacity.

If you're measuring pre-reserved throughput or reallocation performance, you've removed allocation from the measurement. Allocation avoidance is SmallVector's value proposition. These benchmarks are asking "how fast is SmallVector when we remove the reason to use SmallVector?"

---

## Part III — The Investigation

### Context

After implementing SmallVector's fast path optimizations (fast/slow split, layout reordering, redundant load elimination), we needed to measure the results. The standard benchmark—SmallVector vs allocating `std::vector`—couldn't help because allocation overhead would swamp any fast-path improvement.

We also wanted to compare against Boost.Container `small_vector`, an established implementation that's been optimized over many years.

### Initial Approach

We built a fast-path isolation benchmark: SmallVector vs `std::vector` with `reserve()`, both operating on pre-allocated storage.

```cpp
void benchmark_fast_path_throughput() {
    constexpr size_t N = 16;
    constexpr size_t ITERATIONS = 100000;
    
    for (size_t iter = 0; iter < ITERATIONS; ++iter) {
        fat_p::SmallVector<int64_t, 16> vec;
        for (size_t i = 0; i < N; ++i) {
            vec.emplace_back(static_cast<int64_t>(i));
        }
        benchmark_sink += vec.size();
    }
}
```

### Observations

**Observation 1:** At small N (≤ InlineCapacity), SmallVector achieved parity with pre-reserved `std::vector`:

```
InlineCap | SmallVector (ns/op) | std::vector* (ns/op) | Ratio
----------|---------------------|----------------------|------
        4 |                3.29 |                 3.34 |  1.01x
        8 |                1.81 |                 1.77 |  0.98x
       16 |                1.04 |                 1.02 |  0.98x
```

**Observation 2:** At large N (pre-reserved), a gap appeared:

```
Operation (N=10000, pre-reserved) | SmallVector | std::vector | Gap
----------------------------------|-------------|-------------|-----
push_back                         |    0.84 ns  |    0.72 ns  | 17%
emplace_back                      |    0.85 ns  |    0.72 ns  | 18%
```

### Hypotheses

**Hypothesis A:** The gap comes from `allocator_traits::construct()` overhead. Bypassing it with direct placement new would close the gap.

**Hypothesis B:** The gap comes from code shape differences (inlining decisions, register allocation, branch layout) that don't affect correctness.

**Hypothesis C:** The gap doesn't matter because the benchmark measures a scenario where SmallVector provides no value.

### Evidence

**Fact:** The gap appears only in pre-reserved scenarios at large N. In the intended use case (N ≤ InlineCapacity, no reserve), SmallVector achieves parity.

**Fact:** The inline-vs-heap benchmark shows 5-10x wins, confirming SmallVector's value proposition is intact:

```
Size | SmallVector (ns/op) | std::vector (ns/op) | Speedup
-----|---------------------|---------------------|--------
   4 |                4.34 |               22.80 |   5.25x
   8 |                2.31 |               16.66 |   7.23x
  16 |                1.16 |               11.17 |   9.62x
```

**Fact:** Bypassing `allocator_traits::construct()` breaks the instrumented allocator test (Appendix A). The construct count becomes wrong.

**Fact (source audit):** We audited Boost.Container's source to understand their approach:
- `boost::container::vector::emplace_back` uses `allocator_traits_type::construct()` on the fast path—it does *not* bypass the allocator contract
- `boost::container::small_vector` inherits `vector`'s member functions, so it uses the same `emplace_back`
- Boost's bulk algorithms (`copy_move_algo.hpp`) *do* use `memmove` for trivially copyable types during reallocation and range operations

This means Boost made a deliberate choice: honor the allocator contract in `emplace_back`, but optimize bulk operations with memtransfer. The bulk optimization is what makes Boost competitive in reallocation-heavy benchmarks.

### The Fix

There is no code fix. The "fix" is recognizing that Hypothesis C is correct: **the benchmark measures the wrong thing**.

SmallVector's value proposition is allocation avoidance. A benchmark that removes allocation from the measurement cannot evaluate SmallVector's value. The 17% gap in that benchmark is acceptable because the benchmark is irrelevant to the use case.

### Results

| Benchmark | SmallVector | std::vector | Verdict |
|-----------|-------------|-------------|---------|
| Inline storage (N ≤ 16) | 1.16 ns | 11.17 ns | ✅ 9.6x win |
| Pre-reserved (N = 10000) | 0.84 ns | 0.72 ns | ⚠️ 17% gap (irrelevant) |
| Instrumented allocator | ✅ Passes | ✅ Passes | ✅ Correct |

### Transferable Lessons

1. **Benchmark the value proposition.** If your container avoids allocation, benchmark allocation avoidance. A benchmark that removes allocation measures the wrong thing.

2. **Semantic purchases are hidden.** Bypassing `allocator_traits::construct()` doesn't break any test that uses `std::allocator`. The breakage only appears with instrumented or PMR allocators.

3. **Gaps in irrelevant benchmarks are acceptable.** A 17% gap in a benchmark that measures the wrong thing is not a bug. It's a distraction.

---

## Part IV — Foundations

### Design Rationale

fat_p::SmallVector honors the allocator contract for three reasons:

**1. Correctness over benchmarks**

A container that silently breaks allocator instrumentation is not a correct container. It may pass unit tests (which typically use `std::allocator`) while breaking production infrastructure (which may use instrumented allocators).

**2. The gap doesn't matter in practice**

SmallVector's value proposition is avoiding heap allocation. In benchmarks that measure allocation avoidance, SmallVector wins by 5-10x. The 17% gap only appears in benchmarks that remove allocation—exactly the scenario where SmallVector shouldn't be used.

**3. Library trust is fragile**

When a user discovers their allocator instrumentation is silently broken by a library optimization, they lose trust in the entire library. That trust is not recoverable.

### Rejected Alternatives

**Alternative A: memcpy/memmove relocation for trivially copyable types**

This optimization deserves special attention because, unlike the construct bypass, it's *semantically valid*—and Boost does it.

Boost.Container's bulk algorithms (`copy_move_algo.hpp`) use `memmove` for trivially copyable types during operations like reallocation, copy construction, and range insertion. Since `boost::container::small_vector` inherits from `boost::container::vector`, it gets these optimizations automatically.

```cpp
// Valid optimization - Boost does this, we don't
if constexpr (std::is_trivially_copyable_v<T>) {
    std::memmove(newStorage, oldStorage, mSize * sizeof(T));
} else {
    for (size_t i = 0; i < mSize; ++i) {
        AllocTraits::construct(mAllocator, newStorage + i, 
                               std::move_if_noexcept(oldStorage[i]));
        AllocTraits::destroy(mAllocator, oldStorage + i);
    }
}
```

**Why it's valid:** `memmove` for trivially copyable types is well-defined C++. The objects are trivially copyable, so byte-wise copy produces a valid object. No language contract is violated.

**Why Boost does it:** It's a legitimate performance optimization for `vector`-style containers where reallocation and bulk operations are expected to be common.

**Why we rejected it for SmallVector:** It optimizes for the wrong scenario.

SmallVector's value proposition is *avoiding reallocation entirely* by keeping small collections in inline storage. If you're hitting the reallocation path frequently, you've chosen the wrong inline capacity—or you're using the wrong container.

| Usage Pattern | Reallocation Frequency | memcpy Benefit |
|---------------|------------------------|----------------|
| N ≤ InlineCapacity (correct usage) | Never | Zero |
| N slightly > InlineCapacity (edge case) | Once | Negligible |
| N >> InlineCapacity (wrong container) | Multiple | Measurable but irrelevant |

If someone is pushing 10,000 elements into a `SmallVector<T, 16>`, the problem isn't that reallocation is slow—the problem is they should be using `std::vector` with `reserve()`. Optimizing the reallocation path encourages misuse.

**The pattern:** Both the construct bypass and the memcpy optimization share a failure mode: they optimize for benchmarks that don't represent the container's intended use. Before optimizing, ask: *does this benchmark measure the use case that justifies this container's existence?*

**Alternative B: Bypass `construct()` for `std::allocator` only**

```cpp
if constexpr (std::is_same_v<Allocator, std::allocator<T>>) {
    ::new (slot) T(std::forward<Args>(args)...);
} else {
    AllocTraits::construct(mAllocator, slot, std::forward<Args>(args)...);
}
```

**Rejected because:** It creates behavioral differences between allocator types. Code that works with `std::allocator` might break when switched to an instrumented allocator. This is exactly the "works in test, fails in production" bug we're avoiding.

**Alternative C: Provide a policy template parameter**

```cpp
template<typename T, size_t N, typename Alloc, bool BypassConstruct = false>
class SmallVector;
```

**Rejected because:** Complexity explosion. Every user would need to understand the tradeoff. Most would pick the default without understanding. The correct default is "honor the contract."

### Edge Cases

**Edge case: Element type is itself a container**

When `T` is `std::pmr::vector<int>` or similar, `allocator_traits::construct()` enables uses-allocator construction. The outer container's allocator propagates to the inner container. Bypassing `construct()` breaks this propagation.

**Edge case: Allocator has side effects in `construct()`**

Some allocators use `construct()` for logging, poisoning, or invariant checking. Bypassing `construct()` makes these features silently ineffective.

### Mechanical Audit Checklist

To verify a container honors the allocator contract:

- [ ] Run the instrumented allocator test (Appendix A). Construct count must equal element count.
- [ ] Search for `::new` in the container implementation. It should only appear in allocator-aware paths.
- [ ] Search for `is_trivially_copyable` or `is_trivially_constructible` branches. They should not bypass allocator construction.
- [ ] Create a PMR container of PMR containers. Verify inner containers use the outer allocator.

---

## Design Rules to Internalize

1. **The allocator contract is non-negotiable.** Containers must route construction through `allocator_traits::construct()`. No exceptions for "trivial" types.

2. **Benchmark the value proposition.** If your container avoids allocation, benchmark allocation avoidance. Gaps in other benchmarks may be irrelevant.

3. **Semantic purchases are invisible in default tests.** Bypassing `construct()` passes all tests that use `std::allocator`. The breakage only appears with instrumented allocators.

4. **Gaps in irrelevant benchmarks are acceptable.** A 17% gap in pre-reserved throughput is fine when you're winning 5-10x in allocation avoidance.

5. **Library trust is built on correctness.** Users forgive "slightly slower." They don't forgive "silently broke my allocator instrumentation."

---

## What To Do Now

**If you're implementing a container:**

1. Always use `std::allocator_traits<Alloc>::construct()`. Never use placement new directly.
2. Run the instrumented allocator test (Appendix A) as part of your test suite.
3. Don't add `is_trivially_copyable` branches that bypass the allocator.

**If you're evaluating a container library:**

1. Run the instrumented allocator test against it. If the construct count is wrong, the library bypasses the allocator contract.
2. Create a PMR container of PMR containers. Verify inner containers use the outer allocator.

**If you're chasing benchmark numbers:**

1. Ask: does this benchmark measure the value proposition?
2. If the benchmark removes the feature that justifies the container, the benchmark is irrelevant.
3. Never optimize by violating contracts. The benchmark win evaporates when the silent bug manifests.

**Watch out for:**
- Benchmarks that use `reserve()` when evaluating SmallVector. They're measuring the wrong thing.
- Test suites that only use `std::allocator`. They can't detect allocator contract violations.
- Performance advice that says "bypass X for trivial types." X often matters for non-trivial use cases.

---

## Appendix A: The Instrumented Allocator Test

This test detects whether a container routes construction through the allocator's `construct()` hook.

```cpp
#include <cstddef>
#include <cstdlib>
#include <new>

template<class T>
struct CountingAllocator
{
    using value_type = T;
    
    std::size_t* constructions = nullptr;
    
    CountingAllocator() = default;
    explicit CountingAllocator(std::size_t& c) : constructions(&c) {}
    
    template<class U>
    CountingAllocator(const CountingAllocator<U>& other) 
        : constructions(other.constructions) {}
    
    T* allocate(std::size_t n) { 
        return static_cast<T*>(::operator new(n * sizeof(T))); 
    }
    
    void deallocate(T* p, std::size_t) noexcept { 
        ::operator delete(p); 
    }
    
    template<class U, class... Args>
    void construct(U* p, Args&&... args)
    {
        ++(*constructions);
        ::new (static_cast<void*>(p)) U(std::forward<Args>(args)...);
    }
};

template<class Vec>
bool test_construct_hook_observability()
{
    std::size_t count = 0;
    CountingAllocator<typename Vec::value_type> alloc(count);
    
    Vec v(alloc);
    v.emplace_back(1);
    v.emplace_back(2);
    v.emplace_back(3);
    
    // Construct count must equal element count
    return count == 3;
}
```

**Interpretation:**
- If `count == 3`: The container honors the allocator contract.
- If `count == 0`: The container bypasses `allocator_traits::construct()` entirely.
- If `count < 3`: The container bypasses `construct()` for some types (e.g., "trivially copyable" optimization).

---

## Appendix B: Why PMR Exists

PMR (Polymorphic Memory Resources) solves two problems:

**1. Type erasure:** All PMR containers share the same type regardless of which memory resource they use:

```cpp
std::pmr::vector<int> v1;  // Uses default resource
std::pmr::monotonic_buffer_resource pool;
std::pmr::vector<int> v2{&pool};  // Same type!

v1 = v2;  // Works - same type
```

**2. Automatic propagation:** The `polymorphic_allocator::construct()` method passes the allocator to nested containers:

```cpp
std::pmr::monotonic_buffer_resource pool;
std::pmr::vector<std::pmr::string> strings{&pool};

strings.emplace_back("hello");
// The string automatically uses the same pool as the vector
```

This propagation depends on `allocator_traits::construct()` being called. A container that bypasses `construct()` breaks PMR propagation. Inner containers silently fall back to `std::pmr::get_default_resource()`, defeating the memory architecture.

---

## Appendix C: The Full Optimization History

The investigation documented in this case study was part of a broader SmallVector optimization effort. This appendix provides the full context of optimizations considered, implemented, and rejected.

### The Workflow

The optimization process followed a structured pattern:

1. **Static analysis:** AI assistant (Claude) analyzed the `emplace_back` implementation and suggested potential optimizations
2. **Triage:** Each suggestion was evaluated for correctness, semantic impact, and expected benefit
3. **Implementation:** Safe optimizations were implemented
4. **Measurement:** Benchmarks measured the actual impact
5. **Documentation:** Results were recorded with rationale

### Optimizations Implemented

**1. Fast/Slow Path Split**

The original `emplace_back` checked for reallocation on every call (~70 lines including growth logic). The optimization split this into:
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

| Optimization | Valid? | Rejected Because |
|--------------|--------|------------------|
| memcpy for trivially copyable | ✅ Yes | Optimizes wrong behavior (reallocation-heavy misuse) |
| Direct placement new bypass | ❌ No | Violates allocator contract |
| Optimize large-N performance | N/A | Wrong use case for SmallVector |

### Decision Matrix

| Optimization | Implemented | Boost Does It? | Risk | Rationale |
|--------------|-------------|----------------|------|-----------|
| Fast/slow split | ✅ Yes | Yes | None | Pure refactor, better I-cache |
| Layout reorder | ✅ Yes | N/A | None | Zero-cost cache optimization |
| Cache slot pointer | ✅ Yes | N/A | None | Free micro-optimization |
| Remove assert from fast path | ✅ Yes | N/A | None | No function call overhead |
| memmove for trivial types | ❌ No | ✅ Yes | Medium | Optimizes wrong behavior for SmallVector |
| Direct placement new | ❌ No | ❌ No | High | Violates allocator contract |

The implemented optimizations achieved parity with pre-reserved `std::vector` in the inline fast-path benchmark, with zero behavioral changes and full allocator contract compliance. The memtransfer optimization was rejected despite Boost using it, because it optimizes reallocation—exactly the scenario SmallVector is designed to avoid.

---

## References

1. **Boost.Container source (Boost 1.85.0):**
   - `vector.hpp` — `emplace_back` uses `allocator_traits_type::construct()` on fast path (line ~1905)
   - `small_vector.hpp` — inherits `vector` member functions
   - `copy_move_algo.hpp` — memtransfer paths using `memmove` for trivially copyable types

2. **ISO/IEC 14882:2017.** *Programming Languages — C++.* Section [allocator.uses.construction] — Uses-allocator construction.

3. **Boost.Container documentation.** https://www.boost.org/doc/libs/release/doc/html/container.html

---

*Case Study - The Allocator Contract and the 10% Trap — January 2026*
