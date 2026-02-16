---
doc_id: CG-HASHMAP-001
doc_type: "Companion Guide"
title: "The Hash Map Wars"
fatp_components: ["FastHashMap", "StableHashMap", "AllocationStrategies"]
topics: ["SIMD", "Swiss Table", "cache locality", "tombstone deletion", "reference stability", "allocator policy"]
constraints: ["cache lines", "pointer stability", "allocation overhead", "platform differences"]
cxx_standard: "C++20"
build_modes: ["Debug", "Release"]
last_verified: "2025-12-27"
audience: ["C++ developers", "AI assistants"]
status: "final"
---

# Companion Guide - The Hash Map Wars

## Scope

This guide covers the design philosophy, failed experiments, and architectural decisions behind FAT-P's hash map family: FastHashMap (maximum throughput) and StableHashMap (reference stability). It explains *why* these components exist and *how* they evolved through measurement-driven iteration. The goal is to help you understand which map to choose, what tradeoffs each makes, and what we learned from the experiments that didn't work.

## Not covered

- API details and usage patterns (see User Manual - FastHashMap and User Manual - StableHashMap)
- The slow-miss bug investigation (see Case Study - The Case of the Slow Miss)
- General hash table theory and data structures (see Foundations - Hash Table Architectures)
- Performance measurement methodology (see Handbook - Performance Engineering Methodology)

## Prerequisites

- Basic understanding of hash tables and open addressing (you know what "probe sequence" and "load factor" mean)
- Familiarity with the concept of SIMD (single instruction, multiple data) is helpful but not required
- Understanding of pointer/reference stability concerns in C++ (when iterators and pointers are invalidated)

## Companion Guide Card

**Component:** FastHashMap and StableHashMap  
**Design question:** Why two hash maps? What tradeoffs does each make?  
**Key tradeoff:** Throughput vs reference stability—you can't have both  
**Decision made:** Two separate components optimized for different use cases  
**Rejected alternatives:** Robin Hood displacement (overhead at scale), AVX512 groups (cache inefficient), prefetch on probe (usually wasted)  
**Historical context:** boost::unordered_flat_map (2022) showed what was possible; we learned from their design

## Table of Contents

### The Problem We're Solving
- [The Problem We're Solving](#the-problem-were-solving)

### Part I — The Problems
- [Chapter 1: The Performance Gap Discovery](#chapter-1-the-performance-gap-discovery)
- [Chapter 2: The Reference Stability Problem](#chapter-2-the-reference-stability-problem)
- [Chapter 3: The Hash Quality Trap](#chapter-3-the-hash-quality-trap)
- [Chapter 4: The Platform Divergence Problem](#chapter-4-the-platform-divergence-problem)

### Part II — The Solutions
- [FastHashMap: The Throughput Champion](#fasthashmap-the-throughput-champion)
- [StableHashMap: The Stability Guarantee](#stablehashmap-the-stability-guarantee)

### Part III — The Case Studies
- [Case Study 1: The Failed Prefetch Experiment](#case-study-1-the-failed-prefetch-experiment)
- [Case Study 2: The AVX512 Mirage](#case-study-2-the-avx512-mirage)
- [Case Study 3: The Gemini Disaster](#case-study-3-the-gemini-disaster)
- [Case Study 4: The Multi-AI Bug Hunt](#case-study-4-the-multi-ai-bug-hunt)

### Part IV — Foundations
- [Design Rationale: Why Two Maps?](#design-rationale-why-two-maps)
- [Design Rationale: Why Tombstones by Default?](#design-rationale-why-tombstones-by-default)
- [Design Rationale: The is_avalanching Opt-Out](#design-rationale-the-is_avalanching-opt-out)
- [Rejected Alternative: Robin Hood Displacement](#rejected-alternative-robin-hood-displacement)
- [Rejected Alternative: Overflow Bytes](#rejected-alternative-overflow-bytes)
- [Edge Case: The Frozen Map](#edge-case-the-frozen-map)
- [When to Look Elsewhere](#when-to-look-elsewhere)

### Reference
- [Optimization Attempt Summary](#optimization-attempt-summary)
- [Final Benchmark Results](#final-benchmark-results)
- [Glossary](#glossary)
- [Related Documents](#related-documents)

## Key Takeaway Card

| Principle | One-Line Summary |
|-----------|-----------------|
| **Two maps, not one** | Throughput and reference stability are mutually exclusive—pick the right tool |
| **Measurement over theory** | Prefetch, AVX512, and Robin Hood all looked good on paper but failed in practice |
| **Hash quality matters** | std::hash on Windows is identity for integers—always use a finalizer |
| **Simpler is faster** | boost won by removing complexity, not adding it |
| **AI suggestions need verification** | Gemini's "optimization" was a 3.6× regression on misses |

---

## The Problem We're Solving

In 2024, we needed hash maps for a high-performance computing library. The requirements split into two camps:

**Camp A: Maximum Throughput.** Batch processing pipelines that insert millions of records, query them, and discard. No pointer stability needed. Every nanosecond matters.

**Camp B: Reference Stability.** Long-lived caches where external code holds pointers into the map. Values must not move when the map grows. Correctness trumps raw speed.

Standard library offerings failed both camps. `std::unordered_map` provides reference stability but is slow. Flat hash maps like `absl::flat_hash_map` are fast but invalidate pointers on insert.

We built two components:
- **FastHashMap** — Swiss Table with SIMD, optimized for throughput
- **StableHashMap** — Node-based map with pointer indirection, optimized for stability

This guide tells the story of how they evolved—what we tried, what failed, what worked, and why.

---

# Part I — The Problems

## Chapter 1: The Performance Gap Discovery

### The Obvious Approach

Clone Google's Swiss Table design from `absl::flat_hash_map`. It's the state of the art: SIMD probing, 7-bit fingerprints, triangular probing. Everyone knows it's fast.

We built FastHashMap as a faithful Swiss Table implementation. It compiled. It passed tests. We ran benchmarks.

### The Hidden Constraint

At N=1,000,000 elements:

| Map | Find (ns) | vs boost |
|-----|----------:|----------|
| boost::unordered_flat_map | 9.35 | 1.00× |
| FastHashMap | 18.03 | 0.52× |
| absl::flat_hash_map | 18.03 | 0.52× |
| std::unordered_map | 35.00 | 0.27× |

We matched absl. That was expected—we cloned their design. But boost was **2× faster**.

Both maps use Swiss Tables. Both use SIMD. Both have the same algorithmic complexity. So why was boost winning?

### The Symptoms

The gap was largest on **miss detection**:

```
boost miss:        2.32 ns
FastHashMap miss:  3.80 ns
absl miss:         4.72 ns
```

Misses should be the fast path. You hash the key, scan for an empty slot, and stop. No value to return, no work to do. Yet we were 64% slower than boost on the easiest operation.

### The Cost

In a system doing 10 million lookups per second with 30% miss rate, that's 3 million misses per second. At 1.5 ns per miss overhead, we're burning 4.5 milliseconds of CPU time per second on nothing. That's a core at 0.45% utilization doing no useful work.

Scale that to 100 machines and you're wasting half a CPU-hour per hour.

### What We Learned

boost::unordered_flat_map (released 2022) is the **newest** major hash map implementation. It learned from everyone else's mistakes:

| Predecessor | What boost learned |
|-------------|-------------------|
| absl (2017) | SIMD group probing works |
| tsl::robin_map (2017) | Backward-shift deletion is viable |
| ankerl::unordered_dense (2019) | Robin Hood has overhead at scale |
| llvm::DenseMap (2004) | Tombstones hurt miss detection |

boost cherry-picked the best ideas and rejected the rest. Their design is brutally simple: SIMD probing, linear probing, backward-shift deletion. No tombstones. No displacement tracking. No triangular probing.

Sometimes the best optimization is removing complexity.

*Part IV explains the specific optimizations we adopted and rejected.*

---

## Chapter 2: The Reference Stability Problem

### The Obvious Approach

Use a flat hash map for everything. When someone needs pointer stability, tell them to use `std::unordered_map`.

### The Hidden Constraint

`std::unordered_map` is slow. At N=1,000,000:

| Map | Find (ns) | Insert (ns) | Erase (ns) |
|-----|----------:|------------:|------------:|
| FastHashMap | 3.73 | 7.92 | 4.61 |
| std::unordered_map | 13.36 | 32.58 | 38.63 |

That's 3.6× slower on finds and 8.4× slower on erases. For long-lived caches with heavy read traffic, this is unacceptable.

### The Symptoms

Users who need pointer stability are stuck with a bad choice:
- Use `std::unordered_map` and accept the performance hit
- Use a flat map and carefully track when rehashing invalidates pointers
- Build their own node-based map

### The Cost

The "careful tracking" option is a bug farm. We've seen production systems crash because someone added a map insert in a loop that held a pointer from a previous iteration. The pointer was valid on entry, the insert triggered a rehash, and now the pointer points to freed memory.

### What FAT-P Provides

StableHashMap: a node-based map that uses Swiss Table metadata for fast probing, but stores values in separately-allocated nodes. The control byte array can rehash without moving the node storage.

```cpp
StableHashMap<int, Widget> map;
map.insert(1, Widget{});
Widget* ptr = map.find(1);

for (int i = 2; i < 100000; ++i)
    map.insert(i, Widget{});  // Triggers multiple rehashes

*ptr = Widget{42};  // STILL VALID
```

The tradeoff: every lookup involves pointer indirection. We pay 2-3× slowdown vs FastHashMap to get pointer stability. But we're still faster than `std::unordered_map`.

*Part II explains the mechanism in detail.*

---

## Chapter 3: The Hash Quality Trap

### The Obvious Approach

Trust `std::hash`. It's the standard library. It must be good enough.

### The Hidden Constraint

On MSVC (Windows), `std::hash<int64_t>` is often the **identity function**. The hash of 42 is 42. The hash of 43 is 43.

This is catastrophic for Swiss Tables. The H2 fingerprint (7 bits used for SIMD filtering) comes from the hash. If sequential keys have sequential hashes, their H2 values are nearly identical. This causes:

1. Elevated false-positive rates in SIMD matching
2. More key comparisons per lookup
3. Clustering in probe sequences

### The Symptoms

On Windows with `std::hash`:

| Operation | std::hash | SplitMix64 | Improvement |
|-----------|----------:|----------:|------------:|
| Find | 34.39 ns | 27.86 ns | 19% |
| Erase | 94.50 ns | 19.77 ns | **4.8×** |

Erase was 4.8× slower because tombstone accumulation depends on probe length, which depends on hash quality.

### The Cost

Users on Windows see mysteriously slow performance. They assume the library is slow. They don't realize their hash function is the identity function.

### What FAT-P Provides

A built-in SplitMix64 finalizer that runs on all hashes by default:

```cpp
static constexpr size_t splitmix64(size_t x) noexcept {
    x ^= x >> 30;
    x *= 0xbf58476d1ce4e5b9ULL;
    x ^= x >> 27;
    x *= 0x94d049bb133111ebULL;
    x ^= x >> 31;
    return x;
}
```

Hash functions with good avalanche properties can opt out:

```cpp
struct MyGoodHash {
    using is_avalanching = void;  // Skip the finalizer
    size_t operator()(int64_t x) const { return wyhash(&x, sizeof(x), 0, _wyp); }
};
```

*Part IV explains the is_avalanching detection mechanism.*

---

## Chapter 4: The Platform Divergence Problem

### The Obvious Approach

Write portable C++. Compile everywhere. Done.

### The Hidden Constraint

Platforms differ in ways that affect performance:

| Aspect | Windows (MSVC) | Linux (GCC) |
|--------|----------------|-------------|
| std::hash quality | Poor (identity) | Good (has mixer) |
| SIMD default | SSE2 | AVX2 with -march=native |
| aligned_alloc/free | Requires _aligned_free | Works with std::free |

Code that runs fast on Linux may be 4× slower on Windows. Code that compiles on Linux may crash on Windows.

### The Symptoms

During development, we hit a mysterious crash on Windows:

```
(process 14484) exited with code -1073740940 (0xc0000374)
```

That's `STATUS_HEAP_CORRUPTION`. Root cause: we allocated with `std::aligned_alloc` and freed with `std::free`. On POSIX, this works. On Windows, it corrupts the heap.

```cpp
// WRONG on Windows
ctrl_ = static_cast<uint8_t*>(std::aligned_alloc(64, ctrl_size));
std::free(ctrl_);  // Heap corruption!

// CORRECT
#ifdef _MSC_VER
    _aligned_free(ctrl_);
#else
    std::free(ctrl_);
#endif
```

### The Cost

We shipped a version with this bug. It crashed in production. This is the kind of bug that's invisible in testing but catastrophic in deployment.

### What FAT-P Provides

Platform-specific handling encapsulated in the implementation. Users don't need to know about aligned allocation differences—the library handles it.

The SplitMix64 finalizer is on by default, protecting Windows users. On Linux where std::hash is already good, the finalizer adds negligible overhead.

---

# Part II — The Solutions

## FastHashMap: The Throughput Champion

### The Mechanism

FastHashMap uses a two-array Swiss Table design:

```
┌─────────────────────────────────────────────────────┐
│ ctrl_[0..capacity+16]                               │
│ [H2][H2][H2][EMPTY][DEL]...[SENT][MIRR][MIRR]       │
└─────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────┐
│ slots_[0..capacity]                                 │
│ [Key,Value][Key,Value][  empty  ][  deleted  ]...   │
└─────────────────────────────────────────────────────┘
```

The control byte array stores one byte per slot:
- **0x80-0xFF**: Occupied (7 bits = H2 fingerprint, high bit set)
- **0x00**: Empty
- **0x7E**: Deleted (tombstone)
- **0x7F**: Sentinel (for SIMD reads past array end)

The SIMD probe loads 16 control bytes at once and compares against the target H2:

```cpp
BitMask match(uint8_t h2) const {
    auto needle = _mm_set1_epi8(static_cast<char>(h2));
    auto result = _mm_cmpeq_epi8(ctrl_, needle);
    return BitMask(_mm_movemask_epi8(result));
}
```

This turns 16 comparisons into one SIMD instruction. The resulting bitmask tells you which slots are candidates.

### Guarantees and Non-Guarantees

| Property | Guaranteed? | Notes |
|----------|:-----------:|-------|
| O(1) expected lookup | ✅ | With good hash function |
| O(1) worst-case lookup | ❌ | Adversarial hashes can force linear scan |
| Iterator stability | ❌ | Rehash invalidates iterators |
| Pointer stability | ❌ | Rehash moves values |
| Thread safety | ❌ | External synchronization required |
| Exception safety | ✅ | Strong guarantee on insert |

### Decision Guide: Deletion Policy

FastHashMap supports two deletion policies:

**TombstoneDeletion (default):**
- Erase marks slot as deleted, doesn't move elements
- O(1) erase
- Tombstones accumulate, must rehash periodically
- Best for: mixed workloads, insert-heavy patterns

**BackwardShiftDeletion:**
- Erase shifts subsequent elements backward
- O(cluster_length) erase
- No tombstones, never needs tombstone-driven rehash
- Best for: read-heavy workloads with rare deletes

We ran the policy experiment to decide the default:

| Metric | BackwardShift | Tombstone | Winner |
|--------|---------------|-----------|--------|
| Insert | 12.37 ns | 10.44 ns | Tombstone (16% faster) |
| Find | 4.20 ns | 3.37 ns | Tombstone (20% faster) |
| Erase | 30.10 ns | 3.47 ns | **Tombstone (8.7× faster)** |
| Churn | 24.08 ns | 11.72 ns | **Tombstone (2× faster)** |

Tombstone won everything. The backward-shift policy required an 8-byte `home_` field per slot (to track original positions), which added 50% memory overhead and destroyed cache performance.

### Where It Loses

FastHashMap loses to boost::unordered_flat_map on miss detection because boost uses overflow bytes instead of sentinel mirroring. We chose not to implement overflow bytes because:
1. They add complexity
2. The benefit is ~30% on misses specifically
3. Our architecture is already within 2× of boost

FastHashMap also loses when pointer stability is required—that's what StableHashMap is for.

---

## StableHashMap: The Stability Guarantee

### The Mechanism

StableHashMap uses the same control byte array as FastHashMap, but stores **pointers to nodes** instead of inline values:

```
┌─────────────────────────────────────────────────────┐
│ ctrl_[0..capacity+16]                               │
│ [H2][H2][EMPTY][H2][DEL]...                         │
└─────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────┐
│ nodes_[0..capacity]  (pointers, not values)         │
│ [ptr][ptr][null][ptr][null]...                      │
└─────────────────────────────────────────────────────┘

┌─────┐   ┌─────┐   ┌─────┐
│Node1│   │Node2│   │Node3│  ← Heap-allocated, never move
└─────┘   └─────┘   └─────┘
```

When the table rehashes, only the `nodes_` array is reallocated. The actual `Node` objects stay where they are. Pointers into node values remain valid.

### The Tradeoff

Every lookup requires pointer indirection:

```cpp
// FastHashMap: direct access
return &slots_[idx].value;

// StableHashMap: indirect access
return &nodes_[idx]->value;  // Extra memory load
```

At N=1,000,000, this costs roughly 2-3× slowdown vs FastHashMap. But we're still faster than `std::unordered_map` because we use SIMD probing for the control bytes.

### Guarantees and Non-Guarantees

| Property | Guaranteed? | Notes |
|----------|:-----------:|-------|
| Pointer stability | ✅ | Values never move |
| Reference stability | ✅ | References never invalidate |
| Iterator stability | ❌ | Rehash invalidates iterators |
| O(1) expected lookup | ✅ | With good hash function |
| Thread safety | ❌ | External synchronization required |

### Decision Guide: Allocator Policy

StableHashMap supports configurable allocators:

**NewDeleteAllocator (default):**
- Standard heap allocation per node
- Best cache locality for lookups (malloc tends to cluster)
- Best for: read-heavy workloads

**BlockAllocator:**
- Allocates nodes in 256-node blocks
- Faster allocation/deallocation
- Worse cache locality for lookups
- Best for: insert/erase-heavy workloads

| Operation | NewDelete | BlockAllocator | Winner |
|-----------|----------:|---------------:|--------|
| Insert | 39.7 ns | 23.76 ns | Block (1.7×) |
| Find | 17.6 ns | 19.78 ns | NewDelete |
| Miss | 7.6 ns | 27.59 ns | **NewDelete (3.6×)** |
| Erase | 109.1 ns | 36.67 ns | Block (3×) |

BlockAllocator helps inserts but hurts lookups. The scattering of nodes across 256-node blocks destroys cache locality compared to individual allocations (where malloc's allocation patterns create natural clustering).

### Where It Loses

StableHashMap loses to FastHashMap on raw throughput—that's the price of pointer indirection. It loses to specialized node maps like boost::unordered_node_map when they're available.

StableHashMap also loses when the stability guarantee isn't needed. Don't use it just because it "feels safer."

---

# Part III — The Case Studies

## Case Study 1: The Failed Prefetch Experiment

### Context

We hypothesized that prefetching the next probe group would hide memory latency.

### Initial Approach

```cpp
// THE TRAP: Prefetch looks like a free optimization
for (ProbeSequence seq(hash, capacity_); ; seq.next()) {
    __builtin_prefetch(&ctrl_[(seq.offset() + 16) & mask_], 0, 1);
    
    Group group(ctrl_ + seq.offset());
    // ... probe logic ...
}
```

### Observations

No measurable improvement. Sometimes slightly slower.

### Analysis

With a good hash function, most lookups succeed in the **first group**. The prefetch executes, but the probed data is never needed. The prefetch instruction costs cycles (it's not free) and pollutes the cache with data we'll never use.

Prefetch helps when:
- You're likely to need the prefetched data
- You have enough work between prefetch and use to hide latency

Neither condition holds for a fast-path lookup that usually terminates immediately.

### Transferable Lessons

1. **Profile before optimizing.** We assumed the slow path was the problem. It wasn't—the slow path is rare.
2. **Prefetch is not free.** It costs cycles and cache space.
3. **Measure, don't guess.** The "obvious" optimization made things worse.

---

## Case Study 2: The AVX512 Mirage

### Context

AVX512 has 512-bit registers. More lanes = more comparisons per instruction. Should be faster, right?

### Initial Approach

Implement AVX512 probe using 512-bit vectors.

### Observations

Marginal improvement at best. Sometimes regression.

### Analysis

Swiss Table groups are 16 bytes (128-bit). That's the fundamental unit of the algorithm—you probe one group at a time.

AVX512 would require 64-byte groups. This means:
1. More wasted space (63 extra bytes for a single-element table)
2. Worse cache utilization (64 bytes per probe vs 16)
3. The algorithm doesn't vectorize across groups—it vectorizes within a group

What actually helped: compiling with `-mavx2` even when using SSE2 intrinsics. The **VEX encoding** of SSE2 instructions avoids partial register stalls on AVX-capable CPUs. The compilation mode mattered more than the intrinsic width.

### Transferable Lessons

1. **Wider SIMD isn't always better.** Match the SIMD width to the data structure.
2. **Compilation flags can matter more than intrinsics.** VEX vs legacy encoding.
3. **Understand the algorithm before optimizing.** Swiss Tables probe one group at a time.

---

## Case Study 3: The Gemini Disaster

### Context

We asked an AI (Gemini) to review StableHashMap and suggest optimizations.

### Gemini's Suggestions

1. "Add a block allocator for faster allocation"
2. "Add a hash mixer for better distribution"
3. "Add heterogeneous lookup"

### The Problem

All three features **already existed**:
- BlockAllocator was implemented
- SplitMix64 mixer was integrated
- Heterogeneous lookup was available

Gemini hadn't read the code carefully. It suggested features based on what "should" exist, not what actually existed.

### What Gemini Actually Changed

It implemented a block allocator (ignoring the existing one) that scattered nodes across 256-node blocks.

### The Results

| Operation | Before | After Gemini | Change |
|-----------|-------:|-------------:|-------:|
| Miss | 7.6 ns | 27.59 ns | **3.6× slower** |
| Find | 17.6 ns | 19.78 ns | 12% slower |

The "optimization" made misses 3.6× slower. The node scattering destroyed cache locality.

### Transferable Lessons

1. **Verify AI suggestions against existing code.** Grep before implementing.
2. **AI optimizations need benchmarks.** Don't trust suggestions without measurement.
3. **Not all AI reviews are equal.** In our experience, ChatGPT found real bugs; Gemini suggested existing features and made performance worse.

---

## Case Study 4: The Multi-AI Bug Hunt

### Context

We submitted FastHashMap for review by multiple AI systems: Claude, ChatGPT, Gemini, and Grok.

### Results

| AI | Real Bugs Found | False Positives | Useful Suggestions |
|----|-----------------|-----------------|-------------------|
| **ChatGPT** | 12+ | Low | High |
| **Gemini** | 0 | High | Low |
| **Grok** | 0 | Low | Medium |
| **Claude** | 1 | Low | Integration |

### ChatGPT's Critical Finds

**P0: Control Byte Mirroring Bug**

Swiss Table SIMD reads 16 bytes at once. At slot 63 of a 64-slot table, the read spans slots 63-78. Slots 64+ must mirror slots 0-15, not be sentinels.

```cpp
// WRONG: Just fill tail with sentinels
std::memset(ctrl_ + cap, kSentinel, 16);

// THE FIX: Mirror first 16 slots
void set_ctrl(size_t idx, uint8_t value) noexcept {
    ctrl_[idx] = value;
    if (idx < 16) {
        ctrl_[capacity_ + idx] = value;  // Mirror for wraparound reads
    }
}
```

Without mirroring, a key that should be at slot 0 might never be found, causing infinite loops.

**P0: 32-bit Undefined Behavior**

```cpp
// WRONG: UB when sizeof(size_t) == 4
h ^= h >> 33;  // Shift by 33 on 32-bit = undefined behavior

// THE FIX: Platform-aware shift
h ^= h >> (sizeof(size_t) > 4 ? 33 : 16);
```

### What Grok Contributed

Grok suggested adding allocator policy support. This was architecturally sound—it led to the HeapAllocator/FixedAllocator abstraction. But Grok missed all the bugs ChatGPT found.

### Transferable Lessons

1. **Multiple AI reviews catch different things.** No single AI found everything.
2. **ChatGPT excels at bug hunting with counterexamples.** It found 12+ real bugs with specific failure scenarios.
3. **Claude found what others missed.** A namespace bug in Expected.h that didn't manifest in isolated tests.
4. **AI reviews complement, not replace, testing.** The bugs were real—they just needed someone to look.

*For the slow-miss bug found during this investigation, see Case Study - The Case of the Slow Miss.*

---

# Part IV — Foundations

## Design Rationale: Why Two Maps?

We could have built one map with configurable storage. We didn't, for two reasons:

**Reason 1: Clarity of Guarantee**

When you use FastHashMap, you know: "My pointers may invalidate on insert." When you use StableHashMap, you know: "My pointers are safe." There's no configuration to get wrong.

**Reason 2: Zero-Cost Abstraction**

FastHashMap doesn't pay for pointer indirection. StableHashMap doesn't pay for unused tombstone tracking. Each map is optimized for its use case.

The policy template parameters (DeletionPolicy, AllocatorPolicy) handle variations *within* each map's fundamental storage model.

---

## Design Rationale: Why Tombstones by Default?

Theory suggested backward-shift deletion would be better: no tombstone accumulation, guaranteed O(1) probe lengths. The experiment showed otherwise.

The problem was memory overhead. Backward-shift requires tracking each element's "home" position (where it would be if there were no collisions). That's 8 bytes per slot. At N=1,000,000 with 16-byte key-value pairs, that's 50% memory overhead.

The cache penalty from touching 50% more memory dominated any benefit from avoiding tombstone-skipping. Tombstones won.

We kept backward-shift as an option because some workloads (very heavy erase with rare insert) might benefit. But the default is tombstones.

---

## Design Rationale: The is_avalanching Opt-Out

The SplitMix64 finalizer adds a few nanoseconds per hash. For hash functions that already have good avalanche properties (like wyhash or xxHash), this is waste.

We use a trait detection pattern:

```cpp
template<typename T, typename = void>
struct has_is_avalanching : std::false_type {};

template<typename T>
struct has_is_avalanching<T, std::void_t<typename T::is_avalanching>> 
    : std::true_type {};
```

If your hash type has a nested `is_avalanching` type alias, the finalizer is skipped:

```cpp
struct MyGoodHash {
    using is_avalanching = void;  // Any type works; we just detect presence
    size_t operator()(int64_t x) const { return wyhash(...); }
};
```

This is a zero-cost opt-out. Users who know their hash is good can skip the finalizer. Users who don't know get protection by default.

---

## Rejected Alternative: Robin Hood Displacement

Robin Hood hashing tracks how far each element is from its home position. When inserting, if the new element is "poorer" (further from home) than an existing element, they swap. This keeps probe lengths balanced.

We rejected it because:
1. **Memory overhead.** Displacement tracking requires extra bytes per slot.
2. **Complexity.** Robin Hood insertion is more complex than linear probing.
3. **boost proved it unnecessary.** boost::unordered_flat_map is the fastest, and it doesn't use Robin Hood.

The cache effects from memory overhead dominated any benefit from balanced probe lengths.

---

## Rejected Alternative: Overflow Bytes

boost uses "overflow bytes" instead of control byte mirroring. Each group has a byte indicating whether any element overflowed from a previous group.

We rejected it because:
1. **Complexity.** Overflow tracking adds state to maintain.
2. **Our benchmarks are within 2×.** The 30% miss improvement doesn't justify the complexity.
3. **We prioritize auditability.** The mirroring approach is simpler to verify.

If miss performance becomes critical for a specific use case, we may revisit this.

---

## Edge Case: The Frozen Map

StableHashMap supports a `freeze()` method that marks the map read-only. After freezing:
- All mutating operations throw
- Internal state can be optimized for reads

The frozen state survives moves but not copies (copies start unfrozen).

This is useful for maps that are built once and then accessed many times—the freeze makes the "no modifications" invariant explicit and checkable.

---

## When to Look Elsewhere

**If you need lock-free concurrent access:** FAT-P hash maps are not thread-safe. Consider Intel TBB's `concurrent_hash_map` or Facebook's Folly `ConcurrentHashMap`.

**If you need cache-oblivious behavior:** Our maps assume typical cache hierarchies. For NUMA systems or exotic memory configurations, specialized maps may be better.

**If you need ordered iteration:** Hash maps provide no ordering guarantees. Use `std::map` or a B-tree variant.

**If you need persistent storage:** Our maps are in-memory only. Consider RocksDB or LMDB for persistent key-value storage.

---

# Optimization Attempt Summary

| Attempt | Target | Result | Lesson |
|---------|--------|--------|--------|
| Prefetch | Both | ❌ Failed | Most lookups succeed in first group |
| AVX512 | FastHashMap | ⚠️ Marginal | Groups are 16 bytes; wider SIMD doesn't help |
| SIMD for StableHashMap | StableHashMap | ❌ Abandoned | Would break reference stability |
| Backward-Shift | FastHashMap | ⚠️ Mixed | 50% memory overhead killed cache |
| Block Allocator | StableHashMap | ⚠️ Mixed | Helps insert, hurts lookup |
| Gemini's Changes | StableHashMap | ❌ Disaster | 3.6× slower miss |
| SplitMix64 Finalizer | Both | ✅ Success | 12-27% improvement on Windows |
| Deletion Policies | FastHashMap | ✅ Success | Proved Tombstone > BackwardShift |
| Allocator Policies | Both | ✅ Success | Users can choose per workload |
| Control Byte Mirroring | Both | ✅ Critical fix | Prevents infinite loops |

---

# Final Benchmark Results

## FastHashMap vs Competitors (N=1M, Linux, -O3 -march=native)

| Map | Insert | Find | Miss | Erase |
|-----|-------:|-----:|-----:|------:|
| FastHashMap[TS]+SM64 | 7.92 ns | 3.73 ns | 5.15 ns | 4.61 ns |
| boost::unordered_flat_map | 8.12 ns | 3.35 ns | 2.32 ns | 4.89 ns |
| absl::flat_hash_map | 9.45 ns | 4.12 ns | 4.72 ns | 5.23 ns |
| std::unordered_map | 32.58 ns | 13.36 ns | 20.83 ns | 38.63 ns |

FastHashMap beats absl across the board. boost is faster on misses (overflow bytes), but we're competitive elsewhere.

## StableHashMap vs std::unordered_map (N=1M)

| Map | Insert | Find | Miss | Erase |
|-----|-------:|-----:|-----:|------:|
| StableHashMap+SM64 | 27.28 ns | 10.75 ns | 12.05 ns | 22.71 ns |
| StableHashMap[Block]+SM64 | 14.02 ns | 10.20 ns | 11.74 ns | 11.90 ns |
| std::unordered_map | 32.58 ns | 13.36 ns | 20.83 ns | 38.63 ns |

Both StableHashMap configurations beat std::unordered_map while providing the same stability guarantee.

---

# Glossary

**Swiss Table:** Hash table design using SIMD to probe 16 slots simultaneously. Originated at Google (abseil).

**H2 fingerprint:** 7-bit hash fragment stored in control bytes. Used for fast SIMD filtering of candidates.

**Control byte:** One byte per slot indicating state (empty, deleted, or occupied with H2 tag).

**Tombstone:** Marker indicating a deleted slot. Distinguishes "never occupied" from "was occupied, now deleted."

**Backward-shift deletion:** Deletion strategy that shifts subsequent elements backward instead of leaving tombstones.

**Reference stability:** Guarantee that pointers/references to map values remain valid across insertions and rehashes.

**SplitMix64:** Hash finalizer that improves avalanche properties of weak hash functions.

**is_avalanching:** Trait marker indicating a hash function has good avalanche properties and doesn't need finalization.

**Probe sequence:** The sequence of slots visited when searching for a key. Swiss Table uses triangular probing.

---

## Related Documents

- **Case Study - The Case of the Slow Miss** — The miss-path bug found during StableHashMap development
- **User Manual - FastHashMap** — API reference and usage patterns
- **User Manual - StableHashMap** — API reference and usage patterns
- **Handbook - Performance Engineering Methodology** — The discipline behind measurement-driven optimization

---

*Companion Guide: The Hash Map Wars — FAT-P Library, December 2025*

*"Sometimes the best optimization is removing complexity."*
