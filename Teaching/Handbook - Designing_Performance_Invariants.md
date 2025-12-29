# **Handbook - Designing Performance Invariants**

### *Why "Fast" Is Not a Specification*

*FAT-P Library — December 2025*

---

**Scope:** This document teaches how to design, specify, and enforce performance guarantees that survive time. It is not about making code faster—it is about making performance *predictable* and *provable*.

**Audience:** Engineers who have shipped code that was "fast enough" in testing but degraded in production. Engineers who have been burned by benchmarks that measured the wrong thing. Engineers who want performance claims they can defend.

---

# **Table of Contents**

## Part 0 — Foundations

- [What Is an Invariant?](#what-is-an-invariant)
- [Correctness Invariants vs Performance Invariants](#correctness-invariants-vs-performance-invariants)
- [Why Performance Is Harder to Specify](#why-performance-is-harder-to-specify)
- [The Vocabulary of Performance Guarantees](#the-vocabulary-of-performance-guarantees)

## Part I — The Problem

1. [Results vs Guarantees](#chapter-1--results-vs-guarantees)
2. [Why Benchmarks Rot](#chapter-2--why-benchmarks-rot)
3. [The Silent Regression](#chapter-3--the-silent-regression)
4. [When "Fast" Becomes "Slow"](#chapter-4--when-fast-becomes-slow)

## Part II — The Solution

5. [What Makes a Performance Invariant](#chapter-5--what-makes-a-performance-invariant)
6. [The Invariant → Test → CI Pipeline](#chapter-6--the-invariant--test--ci-pipeline)
7. [Structural vs Behavioral Invariants](#chapter-7--structural-vs-behavioral-invariants)
8. [White-Box Testing for Performance](#chapter-8--white-box-testing-for-performance)

## Part III — Practice

9. [Designing Your First Invariant](#chapter-9--designing-your-first-invariant)
10. [Testing Invariants Without Flakiness](#chapter-10--testing-invariants-without-flakiness)
11. [When Invariants Are Impossible](#chapter-11--when-invariants-are-impossible)
12. [Communicating Invariants to Users](#chapter-12--communicating-invariants-to-users)

## Part IV — Principles

- [Appendix A — Invariant Design Checklist](#appendix-a--invariant-design-checklist)
- [Appendix B — Common Invariant Patterns](#appendix-b--common-invariant-patterns)
- [Appendix C — Anti-Patterns](#appendix-c--anti-patterns)

---

# **PART 0 — FOUNDATIONS**

Before discussing performance invariants, you need to understand what invariants are and why performance is uniquely difficult to specify.

---

# **What Is an Invariant?**

An **invariant** is a property that remains true no matter what operations are performed.

The word comes from mathematics: something that doesn't vary. In software, an invariant is a guarantee that holds across all valid states of a system.

## Examples of Invariants

**Data structure invariant:**
> "A binary search tree's left children are always less than their parent."

No matter how many inserts, deletes, or rotations you perform, this property holds. If it ever becomes false, the tree is corrupt.

**API invariant:**
> "After `vector.push_back(x)`, `vector.back() == x`."

This is a contract. Users depend on it. Breaking it breaks their code.

**Resource invariant:**
> "Every allocation has exactly one deallocation."

Memory leaks and double-frees are invariant violations.

## Why Invariants Matter

Invariants convert *intentions* into *guarantees*.

Without invariants, code is held together by coincidence. It works because of how it happens to be used. Change the usage pattern, and it breaks.

With invariants, code is held together by contracts. It works because the contracts are enforced. Change the usage pattern, and the contracts still hold.

## Testing Invariants

The power of an invariant is that it can be *checked*.

```cpp
// After every operation on a BST:
assert(is_valid_bst(tree));  // Invariant check
```

If the check fails, you know exactly what went wrong: the invariant was violated. You don't have to guess. You don't have to reproduce the bug in production. The invariant tells you.

---

# **Correctness Invariants vs Performance Invariants**

Most engineers are familiar with **correctness invariants**:

- "The map returns the value that was inserted"
- "The sorted list is actually sorted"
- "The reference count is never negative"

These are binary: true or false. Either the map returns the right value or it doesn't.

**Performance invariants** are different:

- "Lookup takes O(1) time"
- "Memory usage grows linearly with size"
- "Latency does not degrade over time"

These are *also* binary—but they describe *behavior over time*, not *state at a moment*.

## The Key Difference

Correctness invariants are about **what** happens.
Performance invariants are about **how long** it takes.

Both can be stated precisely. Both can be tested. But performance invariants are harder because:

1. Time is noisy (same operation, different duration)
2. Performance depends on environment (CPU, cache, OS)
3. Degradation is often gradual, not sudden
4. Violations may only appear at scale or over time

---

# **Why Performance Is Harder to Specify**

## Problem 1: Noise

Run the same code twice, get different timings:

```
Run 1: 32 ns
Run 2: 35 ns
Run 3: 31 ns
Run 4: 847 ns  ← Scheduler interrupt
Run 5: 33 ns
```

Which is "the" performance? There isn't one. There's a distribution.

## Problem 2: Environment Dependence

The same code on different machines:

| Machine | Lookup Time |
|---------|-------------|
| Developer laptop | 25 ns |
| CI server | 40 ns |
| Production server | 18 ns |
| Customer's machine | 95 ns |

Which is "the" performance? All of them, in their contexts.

## Problem 3: Gradual Degradation

Performance often doesn't break—it erodes:

```
Day 1:   30 ns
Day 30:  32 ns
Day 90:  38 ns
Day 180: 51 ns
Day 365: 89 ns
```

No single commit broke anything. Each change was "within noise." But the system is now 3× slower.

## Problem 4: Scale Dependence

Performance at small scale doesn't predict large scale:

| Elements | Lookup Time |
|----------|-------------|
| 100 | 15 ns |
| 10,000 | 18 ns |
| 1,000,000 | 25 ns |
| 100,000,000 | 340 ns |

"O(1)" is a lie when your constant factor depends on cache behavior.

## The Consequence

You cannot specify performance the way you specify correctness:

❌ "Lookup takes 30 ns" — Too specific, environment-dependent
❌ "Lookup is fast" — Too vague, unmeasurable
❌ "Lookup is O(1)" — Hides constants that matter in practice

You need a different approach.

---

# **The Vocabulary of Performance Guarantees**

Before designing performance invariants, you need precise language.

## Absolute vs Relative

**Absolute:** "Lookup takes less than 100 ns"
- Fragile. Depends on hardware.
- May be appropriate for hard real-time systems.

**Relative:** "Lookup takes O(1) time"
- Hides constants. "O(1)" can be 1 ns or 1 ms.
- Useful for algorithmic analysis, not practical guarantees.

**Comparative:** "Lookup after 10M operations takes ≤1.25× lookup on fresh table"
- Robust. Compares against self.
- This is the form most performance invariants take.

## Degradation vs Stability

**Degradation:** Performance gets worse over time or operations.
- Tombstone accumulation
- Memory fragmentation
- Cache pollution

**Stability:** Performance remains within bounds regardless of history.
- No tombstones
- Defragmentation on access
- Bounded working set

Performance invariants typically guarantee **stability** (absence of degradation), not **speed** (specific timing).

## Structural vs Behavioral

**Structural:** "There are no tombstones in the table."
- Can be checked by inspecting internal state.
- Binary: either there are tombstones or there aren't.

**Behavioral:** "Lookup latency is stable over time."
- Checked by measuring operations.
- Statistical: requires multiple samples, tolerances.

Structural invariants are stronger because they explain *why* behavior is good, not just *that* it is.

## Invariant Strength Hierarchy

From weakest to strongest:

1. **Benchmark result:** "We measured 30 ns" — Historical fact, not a guarantee
2. **Soft expectation:** "Lookup is usually fast" — No commitment
3. **Behavioral claim:** "Lookup does not degrade" — Measurable but noisy
4. **Structural invariant:** "No tombstones accumulate" — Checkable, deterministic
5. **Enforced invariant:** "CI fails if tombstones detected" — Automated, reliable

The goal is to push claims as high up this hierarchy as possible.

---

# **PART I — THE PROBLEM**

Why benchmarks alone are insufficient, and what goes wrong when performance isn't specified as an invariant.

---

# **CHAPTER 1 — Results vs Guarantees**

## What Most Libraries Ship

Most performance claims look like this:

> "Our hash map achieves 30 ns lookups, 2× faster than std::unordered_map."

This is a **result**. It describes what happened on one machine, at one time, under one workload.

Results are valuable. They inform decisions. But they are not guarantees.

## What Results Don't Tell You

A result doesn't tell you:

- Will this hold on my hardware?
- Will this hold at my scale?
- Will this hold after my workload pattern?
- Will this hold in six months when someone "optimizes" the code?
- Will this hold when the compiler changes?

A result is a snapshot. Production is a movie.

## What a Guarantee Looks Like

A **guarantee** is different:

> "Lookup performance after arbitrary insert/erase churn remains within 25% of lookup performance on a fresh table."

This is not a single measurement. It's a *property* that holds across all states.

## The Difference in Practice

**Result-based thinking:**
1. Write code
2. Benchmark it
3. Publish the numbers
4. Hope it stays fast

**Guarantee-based thinking:**
1. Define what "fast" means precisely
2. Identify what could violate it
3. Design the system to prevent violations
4. Test that violations are impossible
5. Enforce in CI

The second approach is more work. It's also the only approach that survives time.

---

# **CHAPTER 2 — Why Benchmarks Rot**

Benchmarks have a half-life. The numbers you publish today will be wrong tomorrow.

## Reason 1: The Code Changes

Every optimization, refactor, or feature addition can affect performance. Without invariants, these changes are undetected until someone complains.

```cpp
// Original: 30 ns lookup
Value* find(const Key& k) {
    return find_impl(k);
}

// "Improved" with logging: 450 ns lookup
Value* find(const Key& k) {
    LOG_DEBUG("Finding key: " << k);  // Oops
    return find_impl(k);
}
```

No test caught this. The logging looked harmless. Performance degraded 15×.

## Reason 2: The Environment Changes

Benchmarks are environment-specific. When the environment changes, the numbers change.

| Change | Effect |
|--------|--------|
| New CPU | Different cache sizes, branch prediction |
| New compiler | Different optimization decisions |
| New OS | Different scheduler, memory allocator |
| New workload | Different access patterns |

A benchmark from 2023 on Intel may be meaningless in 2025 on ARM.

## Reason 3: The Comparison Changes

Benchmarks often compare against a baseline ("2× faster than X"). But X also changes.

```
2023: Our library is 2× faster than std::unordered_map
2024: std::unordered_map gets optimized
2025: Our library is 0.8× faster than std::unordered_map (i.e., slower)
```

The code didn't change. The claim became false anyway.

## Reason 4: The Measurement Changes

How you measure affects what you measure.

```cpp
// 2023 benchmark: measures best case
for (auto _ : state) {
    map.find(existing_key);  // Always hits
}

// 2024 benchmark: measures realistic case
for (auto _ : state) {
    map.find(random_key);  // Mix of hits and misses
}
```

Same library, different methodology, different conclusions.

## The Rot Is Inevitable

Benchmarks rot because the world changes. The only defense is invariants that are:

- Independent of absolute performance
- Independent of specific environments
- Testable in any environment
- Enforced automatically

---

# **CHAPTER 3 — The Silent Regression**

The most dangerous performance bugs are the ones that don't cause failures.

## How It Happens

1. **Initial state:** Lookup is 30 ns
2. **Small change:** Lookup becomes 33 ns (10% slower, "within noise")
3. **Another change:** Lookup becomes 37 ns (another 12%)
4. **Six months later:** Lookup is 85 ns (2.8× slower)
5. **User complaint:** "Why is your library so slow?"

No single commit was obviously wrong. No test failed. No alert fired.

## Why It's Silent

Most CI systems don't test performance. They test correctness:

- Does the function return the right value? ✓
- Does it handle edge cases? ✓
- Does it not crash? ✓
- Is it still fast? (not tested)

Performance is assumed, not verified.

## The Compounding Effect

Small regressions compound:

| Change | Individual Impact | Cumulative |
|--------|-------------------|------------|
| Added bounds check | +3% | 1.03× |
| Added logging hook | +5% | 1.08× |
| Changed allocator | +8% | 1.17× |
| Added metrics | +4% | 1.22× |
| Refactored hot path | +12% | 1.37× |

Each change was "small." The total is 37% slower.

## Detection Requires Invariants

Without an invariant, you can't detect regression:

- "It's slower than before" requires knowing "before"
- "It's slower than it should be" requires knowing "should be"
- "It's too slow" requires defining "too slow"

An invariant provides the reference point:

> "Aged lookup ≤ 1.25× fresh lookup"

Now regression is detectable. Now CI can fail. Now the bug is caught before release.

---

# **CHAPTER 4 — When "Fast" Becomes "Slow"**

Performance is not a static property. It changes with state.

## The Tombstone Trap (Case Study)

A hash table with tombstone-based deletion:

**Fresh table:**
- Lookup: 30 ns
- Insert: 25 ns
- Erase: 15 ns

**After 10M insert/erase cycles:**
- Lookup: 180 ns (6× slower)
- Insert: 95 ns (3.8× slower)
- Erase: 15 ns (unchanged)

What happened?

Tombstones accumulated. The table is 62% tombstones. Every lookup probes through dead slots. Performance degraded.

**The benchmark showed:** 30 ns lookup (fresh table)
**Production experienced:** 180 ns lookup (aged table)
**The benchmark was true.** It was also misleading.

## The Fragmentation Trap (Case Study)

A custom allocator with a free list:

**Fresh allocator:**
- Allocation: 5 ns
- Deallocation: 3 ns

**After allocation/deallocation churn:**
- Allocation: 850 ns (searching fragmented free list)
- Deallocation: 3 ns (unchanged)

The allocator didn't break. It degraded.

## The Cache Pollution Trap (Case Study)

A tree structure with node pointers:

**Tree fits in L3 cache:**
- Traversal: 15 ns/node

**Tree exceeds L3 cache:**
- Traversal: 95 ns/node (cache misses)

The algorithm is still O(log n). The constant factor changed by 6×.

## The Common Pattern

All these failures share a pattern:

1. **Benchmark measured best case** (fresh, small, fits in cache)
2. **Production encountered worst case** (aged, large, cache pressure)
3. **No invariant guarded against degradation**

The fix is not "better benchmarks." It's structural invariants that prevent the degradation from occurring.

---

# **PART II — THE SOLUTION**

How to design, test, and enforce performance invariants.

---

# **CHAPTER 5 — What Makes a Performance Invariant**

Not every performance claim can be an invariant. Invariants have specific properties.

## The Five Requirements

A performance invariant must be:

### 1. Precisely Stated

❌ "Lookup is fast"
✓ "Lookup after N operations completes in ≤ 1.25× the time of lookup on a fresh table of equal size"

Precise means:
- Quantified (numbers, not adjectives)
- Scoped (what operations, what conditions)
- Measurable (can be checked programmatically)

### 2. State-Independent

❌ "Lookup takes 30 ns"
✓ "Lookup time does not depend on operation history"

The invariant holds regardless of how you got to the current state. Any sequence of valid operations preserves the invariant.

### 3. Operation-Order-Independent

❌ "If you insert before erasing, lookup is fast"
✓ "Lookup performance is stable under arbitrary insert/erase interleaving"

Users will do things in orders you didn't anticipate. The invariant must survive all orderings.

### 4. Mechanically Checkable

❌ "The implementation is cache-friendly"
✓ "Occupied slots == logical size (no tombstones)"

If a human must judge, it's not an invariant. Invariants are checked by code.

### 5. Fails Loudly When Violated

❌ Invariant holds, but no test verifies it
✓ CI fails if invariant is violated

An unenforced invariant is a wish, not a guarantee.

## Good Invariants vs Bad Invariants

| Bad (Not an Invariant) | Good (Invariant) |
|------------------------|------------------|
| "Lookups are fast" | "Probe distance ≤ 20 at 0.75 load factor" |
| "Memory usage is reasonable" | "Memory = O(n) with coefficient ≤ 3" |
| "No degradation" | "Aged/fresh latency ratio ≤ 1.25" |
| "Cache-efficient" | "All entries in contiguous array" |
| "Lock-free" | "No operation acquires a mutex" |

The good invariants are checkable. The bad ones are opinions.

---

# **CHAPTER 6 — The Invariant → Test → CI Pipeline**

An invariant without enforcement is documentation. Enforcement requires a pipeline.

## The Pipeline

```
Claim
  ↓
Invariant
  ↓
Test
  ↓
CI Failure
```

Each step transforms the previous:

### Step 1: Claim → Invariant

Start with what you want to promise:

**Claim:** "StableHashMap does not degrade under churn"

Convert to precise invariant:

**Invariant:** "After N insert/erase operations, lookup latency is within 25% of fresh-table lookup latency"

### Step 2: Invariant → Test

Write a test that checks the invariant:

```cpp
TEST(ChurnStability, LatencyIsStable) {
    Map map;
    fill(map, 50000);
    
    auto fresh_time = measure_lookups(map);
    
    churn(map, 200000);  // 4× the size in operations
    
    auto aged_time = measure_lookups(map);
    
    double ratio = aged_time / fresh_time;
    ASSERT_LT(ratio, 1.25);  // Invariant check
}
```

### Step 3: Test → CI Failure

Configure CI to run the test:

- Test passes → Build succeeds
- Test fails → Build fails, commit blocked

Now the invariant is enforced. Every change that violates it is caught.

## Why Each Step Matters

| Missing Step | Consequence |
|--------------|-------------|
| No invariant | Vague promise, can't be tested |
| No test | Invariant might be violated, nobody knows |
| No CI | Test exists but isn't run, violations slip through |

All three steps are required. Skip one, and the guarantee evaporates.

## Example: StableHashMap's Pipeline

**Claim:** "No tombstones"

**Invariant:** "Occupied slots == logical size"

**Test:**
```cpp
TEST(NoTombstones, OccupiedEqualsSize) {
    Map map;
    fill_and_churn(map, 100000);
    
    size_t occupied = count_occupied_slots(map);  // White-box
    size_t logical = map.size();
    
    ASSERT_EQ(occupied, logical);
}
```

**CI:** Test runs on every commit. Failure blocks merge.

If anyone accidentally reintroduces tombstones, the build breaks. The invariant is protected.

---

# **CHAPTER 7 — Structural vs Behavioral Invariants**

There are two kinds of performance invariants. Understanding the difference helps you choose the right approach.

## Behavioral Invariants

**Definition:** A property of observable behavior (timing, throughput, latency).

**Example:** "Aged lookup is within 25% of fresh lookup"

**How to test:** Measure operations, compare results.

```cpp
auto fresh = measure_lookups(fresh_map);
auto aged = measure_lookups(aged_map);
ASSERT_LT(aged / fresh, 1.25);
```

**Strengths:**
- Tests what users actually experience
- Doesn't require internal access
- Directly answers "is it fast enough?"

**Weaknesses:**
- Noisy (timing varies)
- May need tolerances (flaky tests)
- Doesn't explain *why* performance is good/bad
- Can be fooled (fast for wrong reasons)

## Structural Invariants

**Definition:** A property of internal state that implies good performance.

**Example:** "No tombstones exist" (implies no probe-chain degradation)

**How to test:** Inspect internal state directly.

```cpp
size_t occupied = count_occupied_slots(map);
ASSERT_EQ(occupied, map.size());
```

**Strengths:**
- Deterministic (no noise)
- Explains the mechanism
- Catches problems before they manifest
- Can't be fooled

**Weaknesses:**
- Requires internal access (breaks encapsulation)
- Must correctly identify the structural cause
- Doesn't directly prove performance (only implies it)

## Which to Use?

**Use structural invariants when:**
- You know the structural cause of performance
- You can access internals (friend classes, test builds)
- You want deterministic, non-flaky tests
- You want to catch problems at the root

**Use behavioral invariants when:**
- The structural cause is unknown or complex
- You can't access internals
- You need to test the actual user experience
- The behavior is what matters, not the mechanism

**Best practice:** Use both. Structural invariants explain *why*. Behavioral invariants confirm *that*.

## StableHashMap Example

**Structural invariants:**
- No tombstones (occupied == size)
- Probe distance bounded (avg ≤ expected for load factor)

**Behavioral invariant:**
- Aged/fresh latency ratio ≤ 1.25

The structural invariants *imply* the behavioral one. If there are no tombstones and probe distances are bounded, latency *must* be stable.

Testing all three provides defense in depth:
- If the structural tests pass but behavioral fails → Something else is wrong
- If structural fails → We know exactly what broke
- If behavioral fails but structural passes → The invariant model is incomplete

---

# **CHAPTER 8 — White-Box Testing for Performance**

Structural invariants require inspecting internal state. This means breaking encapsulation. Done wrong, this is an anti-pattern. Done right, it's essential.

## Why White-Box Testing Is Necessary

Black-box testing only sees behavior:

```cpp
// Black-box: can only observe timing
auto time = measure_lookup(map);
```

White-box testing sees structure:

```cpp
// White-box: can see why timing is what it is
auto tombstones = count_tombstones(map);
auto avg_probe = average_probe_distance(map);
```

If timing is bad, black-box testing tells you *that* it's bad.
White-box testing tells you *why*.

## The Encapsulation Concern

"But breaking encapsulation is bad!"

Yes—for production code. Tests are different.

| Context | Encapsulation Priority |
|---------|------------------------|
| Production API | High (users depend on stability) |
| Unit tests | Medium (test behavior, not implementation) |
| Invariant tests | Low (testing the implementation *is the point*) |

Performance invariants are about implementation. You cannot test them without seeing the implementation.

## The Right Way to Break Encapsulation

### Pattern: Friend Test Class

```cpp
// In header
template <typename K, typename V, typename P>
class StableHashMap {
    // ...
    template <typename>
    friend class testing::StableHashMapTester;
    
private:
    std::vector<Entry> buckets_;
    size_t mask_;
};

// In test file
template <typename MapType>
class StableHashMapTester {
public:
    static size_t count_occupied(const MapType& map) {
        size_t count = 0;
        for (const auto& e : map.buckets_) {
            if (e.hash != 0) count++;
        }
        return count;
    }
    
    static double avg_probe_distance(const MapType& map) {
        // ... access map.buckets_, map.mask_
    }
};
```

**Why this is correct:**
- Friend declaration is explicit (intentional exposure)
- Only test code has access (production code unchanged)
- Accessor is read-only (can't corrupt state)
- Tests are in separate file (clear separation)

### Pattern: Test-Only Accessors

```cpp
#ifdef TESTING
public:
    const std::vector<Entry>& debug_buckets() const { return buckets_; }
    size_t debug_mask() const { return mask_; }
#endif
```

**Why this is correct:**
- Accessors only exist in test builds
- Production builds have full encapsulation
- Clear naming signals "not for production use"

## What White-Box Tests Should Check

| Invariant | What to Inspect |
|-----------|-----------------|
| No tombstones | occupied_slots == size() |
| Probe distance | avg_distance <= expected_for_load_factor |
| Memory layout | entries are contiguous |
| No fragmentation | free_list.size() <= threshold |
| Alignment | pointer % alignment == 0 |

## What White-Box Tests Should NOT Do

❌ Modify internal state
❌ Test implementation details that could validly change
❌ Make tests dependent on specific internal values
❌ Expose internals to production code

The goal is to verify invariants, not to couple tests to implementation details.

---

# **PART III — PRACTICE**

Applying invariant thinking to real engineering.

---

# **CHAPTER 9 — Designing Your First Invariant**

A step-by-step process for converting a vague performance goal into an enforceable invariant.

## Step 1: Identify the Failure Mode

What bad thing are you trying to prevent?

**Vague:** "The system should stay fast"
**Specific failure mode:** "Lookup degrades as tombstones accumulate"

If you can't name the failure mode, you can't design an invariant against it.

## Step 2: Identify the Root Cause

Why does the failure mode occur?

**Failure:** Lookup degrades over time
**Root cause:** Tombstones accumulate, probe chains lengthen

The root cause suggests the structural invariant.

## Step 3: State the Structural Invariant

What structural property, if maintained, prevents the failure?

**Root cause:** Tombstones accumulate
**Structural invariant:** "No tombstones exist (occupied == size)"

This is the strongest form of the invariant—it prevents the root cause entirely.

## Step 4: State the Behavioral Invariant

What observable behavior should result?

**Structural invariant:** No tombstones
**Behavioral invariant:** "Aged lookup ≤ 1.25× fresh lookup"

This is the user-visible consequence.

## Step 5: Define Measurement Conditions

Under what conditions will you check the invariant?

- **Table size:** 50,000 elements
- **Churn amount:** 200,000 operations (4× size)
- **Measurement:** 1M lookups, median time
- **Tolerance:** 25% (ratio ≤ 1.25)

Be specific. Vague conditions lead to flaky tests.

## Step 6: Write the Test

```cpp
TEST(ChurnStability, NoTombstones) {
    Map map;
    fill_and_churn(map, 50000, 200000);
    
    // Structural
    ASSERT_EQ(count_occupied(map), map.size());
}

TEST(ChurnStability, LatencyStable) {
    Map map;
    fill(map, 50000);
    auto fresh = measure_lookups(map, 1000000);
    
    churn(map, 200000);
    auto aged = measure_lookups(map, 1000000);
    
    // Behavioral
    ASSERT_LT(aged / fresh, 1.25);
}
```

## Step 7: Add to CI

Ensure the test runs on every commit. Ensure failure blocks the build.

## Worked Example: Memory Growth

**Goal:** "Memory usage should be reasonable"

**Step 1 — Failure mode:** Memory grows without bound

**Step 2 — Root cause:** Allocator doesn't return memory to OS; fragmentation

**Step 3 — Structural invariant:** "Allocated memory ≤ 3× logical data size"

**Step 4 — Behavioral invariant:** "RSS after 1M operations ≤ 3× minimum required"

**Step 5 — Conditions:**
- Insert 100K elements (known size)
- Churn 500K operations
- Measure RSS

**Step 6 — Test:**
```cpp
TEST(MemoryStability, BoundedGrowth) {
    Map map;
    size_t element_size = sizeof(Key) + sizeof(Value);
    size_t expected_min = 100000 * element_size;
    
    fill(map, 100000);
    churn(map, 500000);
    
    size_t actual = get_rss();
    ASSERT_LT(actual, expected_min * 3);
}
```

---

# **CHAPTER 10 — Testing Invariants Without Flakiness**

Performance tests are notorious for flakiness. Here's how to write reliable ones.

## The Flakiness Problem

```
Monday:    PASSED (ratio = 1.18)
Tuesday:   PASSED (ratio = 1.21)
Wednesday: FAILED (ratio = 1.27)  ← CI blocked
Thursday:  PASSED (ratio = 1.19)  ← Same code!
```

The test didn't change. The code didn't change. The result changed.

## Why Performance Tests Flake

| Cause | Effect |
|-------|--------|
| CPU throttling | Measurements slower than expected |
| Background processes | Interference, cache pollution |
| Memory pressure | Swapping, slower allocation |
| Scheduler decisions | Variable latency |
| Cache state | Cold vs warm measurements |

## Strategy 1: Test Structure, Not Timing

Structural invariants don't flake:

```cpp
// Never flaky: deterministic check
ASSERT_EQ(count_occupied(map), map.size());

// Sometimes flaky: timing-dependent
ASSERT_LT(aged_time / fresh_time, 1.25);
```

Where possible, convert behavioral invariants to structural ones.

## Strategy 2: Use Ratios, Not Absolutes

```cpp
// Flaky: depends on machine speed
ASSERT_LT(lookup_time, 50);  // ns

// Less flaky: self-relative
ASSERT_LT(aged / fresh, 1.25);
```

Ratios are machine-independent. The same ratio should hold on fast and slow machines.

## Strategy 3: Generous Tolerances

If your invariant is "no degradation," don't test for ratio = 1.0.

```cpp
// Too tight: will flake
ASSERT_LT(ratio, 1.05);

// Appropriate: catches real problems
ASSERT_LT(ratio, 1.25);

// Too loose: misses real problems
ASSERT_LT(ratio, 2.0);
```

The tolerance should be:
- Tight enough to catch real regressions (3× slowdown)
- Loose enough to ignore noise (10% variation)

## Strategy 4: Multiple Samples

Don't trust a single measurement:

```cpp
std::vector<double> ratios;
for (int trial = 0; trial < 5; trial++) {
    Map map;
    // ... setup and measure
    ratios.push_back(aged / fresh);
}

double median_ratio = median(ratios);
ASSERT_LT(median_ratio, 1.25);
```

Median is robust to outliers. A single bad run doesn't cause failure.

## Strategy 5: Warm Up

```cpp
// Warmup: don't measure this
for (int i = 0; i < 3; i++) {
    measure_lookups(map);  // Discard
}

// Measurement: measure this
auto time = measure_lookups(map);
```

Warmup eliminates cold-cache effects that vary between runs.

## Strategy 6: Accept Some Flakiness in CI

If a test flakes once per 100 runs, and CI runs 10 times per day:
- Expected flakes: 1 per 10 days
- Acceptable? Maybe.

Perfect reliability is impossible. Set expectations appropriately.

## The Nuclear Option: Separate Performance CI

If performance tests are too flaky for blocking CI:

1. Run them in a separate, non-blocking job
2. Alert on failures instead of blocking
3. Investigate trends over time

This is a compromise, but it's better than not testing at all.

---

# **CHAPTER 11 — When Invariants Are Impossible**

Not everything can be an invariant. Knowing when to stop is wisdom.

## Situation 1: Hardware Dependence

**Claim:** "SIMD operations are used"

**Why it's not an invariant:** Whether SIMD is used depends on compiler, flags, and CPU features. The same code may use SIMD on one machine and scalar on another.

**Alternative:** Document the conditions under which SIMD is used. Provide a runtime check:

```cpp
bool is_simd_enabled() {
    return SIMD_AVAILABLE && sizeof(Key) == 8;
}
```

## Situation 2: Workload Dependence

**Claim:** "Cache-efficient for typical workloads"

**Why it's not an invariant:** "Typical" is undefined. Some workloads will be cache-efficient, others won't.

**Alternative:** Define specific workload classes and characterize performance for each:

| Workload | Expected Performance |
|----------|---------------------|
| Sequential scan | 2 ns/element |
| Random access | 15 ns/element |
| Heavy churn | Stable over time |

## Situation 3: External Dependencies

**Claim:** "Allocation is fast"

**Why it's not an invariant:** Allocation speed depends on the allocator, which is external. The same code with different allocators has different performance.

**Alternative:** Document allocator assumptions. Provide benchmarks with specific allocators.

## Situation 4: Gradients, Not Thresholds

**Claim:** "Performance degrades gracefully under load"

**Why it's not an invariant:** "Gracefully" is subjective. There's no binary threshold.

**Alternative:** Characterize the degradation curve:

| Load Factor | Expected Probe Distance |
|-------------|------------------------|
| 0.50 | 1.0 |
| 0.75 | 1.5 |
| 0.90 | 5.0 |
| 0.95 | 10.0 |

This is documentation, not an invariant—but it's honest.

## Situation 5: Inherent Tradeoffs

**Claim:** "Both insertion and deletion are fast"

**Why it might not be an invariant:** Some data structures trade off insertion vs deletion speed. You can't have both be optimal.

**Alternative:** Be explicit about the tradeoff:

> "Insertion is O(1) amortized. Deletion is O(probe_distance) due to backward shift. For workloads with >50% deletions, consider [alternative]."

## The Meta-Lesson

Not everything deserves an invariant. Over-specifying performance leads to:

- Brittle tests
- False confidence
- Wasted engineering effort

Reserve invariants for properties that:
- Are critical to correctness or usability
- Can actually be maintained
- Are worth the cost of enforcement

---

# **CHAPTER 12 — Communicating Invariants to Users**

An invariant that users don't know about provides no value. Communication is part of the design.

## Where to Document Invariants

### 1. In the Code (Comments)

```cpp
/**
 * @invariant Occupied slots == size() (no tombstones)
 * @invariant Probe distance bounded by O(1) expected at load < 0.875
 */
class StableHashMap { ... };
```

Developers reading the code see the invariants immediately.

### 2. In the API Documentation

> **Performance Guarantees**
> 
> StableHashMap maintains the following invariants:
> - No tombstones accumulate (backward-shift deletion)
> - Probe distance remains bounded under arbitrary churn
> - Lookup performance after 10M operations equals fresh performance (±25%)

Users choosing the library see what they're getting.

### 3. In the Test Suite

```cpp
// Test names communicate invariants
TEST(ChurnStability, NoGhostSlotsAccumulate) { ... }
TEST(ChurnStability, ProbeDistanceDoesNotDegrade) { ... }
TEST(ChurnStability, LatencyIsStable) { ... }
```

The test suite is executable documentation.

### 4. In Failure Messages

```cpp
ASSERT_EQ(occupied, size()) 
    << "INVARIANT VIOLATION: Ghost slots detected. "
    << "Occupied=" << occupied << " Size=" << size()
    << ". This indicates tombstone accumulation.";
```

When invariants fail, the message explains what and why.

## How to Phrase Invariants

### For Engineers (Precise)

> "Occupied slots == logical size, verified after every erase operation"

### For Users (Meaningful)

> "Performance remains stable regardless of insert/erase history"

### For Decision Makers (Comparative)

> "Unlike tombstone-based hash tables, StableHashMap does not degrade under sustained churn"

Match the phrasing to the audience.

## What Not to Communicate

**Don't promise more than you enforce:**

❌ "Lookup is always fast" (not enforced, not precise)
✓ "Lookup is within 25% of fresh-table performance" (enforced, precise)

**Don't communicate internal details users don't need:**

❌ "The mask_ field is ANDed with the hash to compute bucket index"
✓ "Lookup is O(1) expected"

**Don't imply guarantees you don't have:**

❌ "Optimal performance" (compared to what?)
✓ "Non-degrading performance under churn" (specific, testable)

---

# **PART IV — PRINCIPLES**

Checklists and patterns for reference.

---

# **APPENDIX A — Invariant Design Checklist**

Use this checklist when designing a performance invariant.

## Before Defining the Invariant

- [ ] Identify the failure mode you're preventing
- [ ] Identify the root cause of the failure mode
- [ ] Confirm the failure mode is worth preventing (cost/benefit)
- [ ] Confirm the root cause can be eliminated or bounded

## Defining the Invariant

- [ ] State the structural invariant (what internal property holds)
- [ ] State the behavioral invariant (what users observe)
- [ ] Ensure the invariant is precisely stated (quantified, scoped)
- [ ] Ensure the invariant is state-independent (holds after any operation sequence)
- [ ] Ensure the invariant is mechanically checkable (no human judgment)

## Testing the Invariant

- [ ] Write a structural test (white-box, deterministic)
- [ ] Write a behavioral test (black-box, timing-based)
- [ ] Define measurement conditions (size, churn, samples)
- [ ] Define tolerances (tight enough to catch problems, loose enough to avoid flakes)
- [ ] Run tests multiple times to verify stability

## Enforcing the Invariant

- [ ] Add tests to CI
- [ ] Ensure failure blocks the build (or alerts appropriately)
- [ ] Document the invariant in code comments
- [ ] Document the invariant in user-facing documentation

## Maintaining the Invariant

- [ ] Review invariants when making changes to the component
- [ ] Update invariants if design changes
- [ ] Remove invariants that are no longer valid or useful

---

# **APPENDIX B — Common Invariant Patterns**

Reusable patterns for common performance concerns.

## Pattern: No Degradation Over Time

**Failure mode:** Performance degrades as operations accumulate

**Structural invariant:** [Cause-specific, e.g., "no tombstones"]

**Behavioral invariant:** "aged_metric / fresh_metric ≤ 1.25"

**Test structure:**
```cpp
auto fresh = measure(fresh_state);
auto aged = measure(after_operations);
ASSERT_LT(aged / fresh, 1.25);
```

## Pattern: Bounded Growth

**Failure mode:** Resource usage grows without bound

**Structural invariant:** "resource ≤ k × minimum_required"

**Behavioral invariant:** Same as structural

**Test structure:**
```cpp
size_t minimum = calculate_minimum(data);
size_t actual = measure_resource();
ASSERT_LT(actual, minimum * k);
```

## Pattern: Complexity Bound

**Failure mode:** Operation exceeds expected complexity

**Structural invariant:** [Algorithm-specific, e.g., "tree is balanced"]

**Behavioral invariant:** "time(n) / time(n/2) ≤ expected_ratio"

**Test structure:**
```cpp
auto time_n = measure(n);
auto time_2n = measure(2 * n);
double ratio = time_2n / time_n;
// For O(n): ratio ≈ 2
// For O(n log n): ratio ≈ 2.1-2.2
// For O(n²): ratio ≈ 4
ASSERT_LT(ratio, expected_ratio * 1.25);
```

## Pattern: No Pathological Cases

**Failure mode:** Certain inputs cause extreme slowdown

**Structural invariant:** [Cause-specific, e.g., "max probe distance < 50"]

**Behavioral invariant:** "worst_case / average_case ≤ k"

**Test structure:**
```cpp
auto avg = measure_average_case();
auto worst = measure_pathological_case();
ASSERT_LT(worst / avg, k);
```

## Pattern: Resource Release

**Failure mode:** Resources not released when expected

**Structural invariant:** "after clear(), allocated_memory ≈ baseline"

**Behavioral invariant:** Same as structural

**Test structure:**
```cpp
size_t baseline = measure_memory();
fill(container, n);
container.clear();
size_t after_clear = measure_memory();
ASSERT_LT(after_clear, baseline * 1.1);
```

---

# **APPENDIX C — Anti-Patterns**

What not to do.

## Anti-Pattern: Vague Invariants

❌ "The system is fast"
❌ "Performance is acceptable"
❌ "Latency is low"

These cannot be tested. They are wishes, not invariants.

## Anti-Pattern: Absolute Thresholds

❌ "Lookup takes less than 50 ns"

Absolute thresholds are machine-dependent. They fail on slow machines and pass on fast ones, regardless of actual quality.

## Anti-Pattern: Testing Implementation Details

❌ "The hash table has exactly 16 buckets at size 10"

This couples tests to implementation decisions that could validly change.

## Anti-Pattern: Ignoring Noise

❌ `ASSERT_EQ(aged_time, fresh_time);`

Performance measurements are noisy. Exact equality will always fail.

## Anti-Pattern: Over-Specification

❌ Testing every internal counter and state variable

Invariants should be meaningful properties, not a complete description of internal state.

## Anti-Pattern: Under-Enforcement

❌ Invariant documented but no test
❌ Test exists but not in CI
❌ CI runs test but doesn't block on failure

An unenforced invariant provides false confidence.

## Anti-Pattern: Invariant Creep

❌ Adding invariants for every property
❌ Never removing invariants
❌ Invariants that don't match the actual design

Invariants require maintenance. Too many become a burden.

---

# **Summary**

## The Core Ideas

1. **Results are not guarantees.** A benchmark measures what happened; an invariant guarantees what will happen.

2. **Benchmarks rot.** Code changes, environments change, comparisons change. Invariants survive.

3. **Performance invariants are properties that hold across all states.** They are precise, state-independent, and mechanically checkable.

4. **Structural invariants explain why.** They identify the internal property that causes good performance.

5. **Behavioral invariants confirm that.** They measure the user-visible consequence.

6. **The pipeline is: Claim → Invariant → Test → CI Failure.** Missing any step breaks the guarantee.

7. **Not everything can be an invariant.** Some properties depend on hardware, workload, or external factors. Document these honestly.

8. **Communicate invariants to users.** An invariant that users don't know about provides no value.

## The Meta-Lesson

Performance engineering is not about making code fast.

It's about **making performance predictable**.

Predictable means:
- You know what to expect
- You know when expectations are violated
- You can trust the system over time

Invariants are how you achieve predictability.

Without invariants, performance is luck.
With invariants, performance is engineering.

---

*FAT-P Library Documentation — December 2025*
