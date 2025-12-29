# **Case Study - Benchmarking Hash Tables Without Lying to Yourself**

### *A Case Study in Performance Methodology*

*FAT-P Library — December 2025*

---

**Scope:** This document covers the methodology developed while benchmarking StableHashMap. It is not a results document—it's a case study in how benchmarks go wrong and how to fix them. The lessons apply to any performance-critical component, not just hash tables.

**Audience:** Engineers writing or evaluating performance-critical components who need defensible benchmarks, not just impressive numbers.

---

# **Table of Contents**

**[Introduction: Why Methodology Matters](#introduction-why-methodology-matters)**

## Part 0 — Foundations

- [What Is a Benchmark?](#what-is-a-benchmark)
- [Hash Table Fundamentals](#hash-table-fundamentals)
- [Swiss Tables: The Modern Approach](#swiss-tables-the-modern-approach)
- [The Competitor Landscape](#the-competitor-landscape)
- [Statistics You Need to Know](#statistics-you-need-to-know)
- [The Measurement Problem](#the-measurement-problem)

## Part I — The Traps

1. [The Stability Assumption](#chapter-1--the-stability-assumption)
2. [API Equivalence Is Not Semantic Equivalence](#chapter-2--api-equivalence-is-not-semantic-equivalence)
3. [Allocation Boundaries Leak](#chapter-3--allocation-boundaries-leak)
4. [Statistics Can Obscure Reality](#chapter-4--statistics-can-obscure-reality)

## Part II — The Fixes

5. [Pointer Stability Verification](#chapter-5--pointer-stability-verification)
6. [Semantic Alignment](#chapter-6--semantic-alignment)
7. [Block Allocator Benchmarks](#chapter-7--block-allocator-benchmarks)
8. [Platform Variance as Signal](#chapter-8--platform-variance-as-signal)

## Part III — The Verification Suite

9. [Reference Stability Tests](#chapter-9--reference-stability-tests)
10. [SIMD Acceleration Verification](#chapter-10--simd-acceleration-verification)
11. [Block Allocator Efficiency Tests](#chapter-11--block-allocator-efficiency-tests)

## Part IV — Principles

- [Appendix A — The Benchmarking Checklist](#appendix-a--the-benchmarking-checklist)
- [Appendix B — Anti-Patterns Catalog](#appendix-b--anti-patterns-catalog)

---

# **Introduction: Why Methodology Matters**

Benchmarking is where most performance engineering goes wrong.

Not because engineers are dishonest—but because:

- Microbenchmarks measure the wrong thing
- Invariants are violated without being noticed
- Results are over-interpreted
- Allocation patterns are ignored

StableHashMap's development forced repeated confrontations with these failure modes. Early benchmarks showed it was "faster" than `std::unordered_map`. Later analysis revealed those benchmarks were measuring allocation patterns, not hash table algorithms. The difference matters.

This case study documents **what went wrong, what was corrected, and what was learned**. The lessons apply beyond hash tables to any component where performance claims must be defensible.

**The core insight:**

> Benchmark results age. Methodology does not.

A benchmark that measures the right thing with honest labels is more valuable than a benchmark that produces impressive numbers through careful selection of favorable conditions.

---

# **PART 0 — FOUNDATIONS**

Before diving into what goes wrong with benchmarks, you need to understand what benchmarks are, how hash tables work, and what statistics actually tell you. If you're already comfortable with these concepts, skip to Part I.

---

# **What Is a Benchmark?**

A benchmark is a controlled experiment that measures how long something takes.

That's it. Everything else—the tooling, the statistics, the methodology—exists to make that measurement *meaningful*.

## Why Benchmarks Exist

You can't improve what you can't measure. When someone asks "is implementation A faster than implementation B?", the only honest answer requires measurement. Intuition fails. Code inspection fails. Only timing tells the truth.

## The Basic Structure

Every benchmark has three parts:

```cpp
// 1. SETUP: Prepare the state
Map map;
for (int i = 0; i < N; ++i) {
    map.insert(i, value);
}

// 2. MEASUREMENT: Time the operation
auto start = now();
for (int i = 0; i < N; ++i) {
    map.find(i);
}
auto end = now();

// 3. REPORTING: Compute and display results
double ns_per_op = (end - start) / N;
std::cout << "Find: " << ns_per_op << " ns/op\n";
```

Setup happens *before* timing. Measurement captures *only* the operation of interest. Reporting converts raw time into something interpretable.

## What Can Go Wrong

Every part can fail:

| Part | Failure Mode | Example |
|------|--------------|---------|
| Setup | Wrong initial state | Measuring empty table instead of full |
| Setup | Setup leaks into measurement | Allocation during "timed" region |
| Measurement | Wrong operation | API does something unexpected |
| Measurement | Compiler optimizes away work | Dead code elimination |
| Measurement | External interference | Other processes, CPU throttling |
| Reporting | Wrong math | Dividing by wrong N |
| Reporting | Wrong statistics | Mean when median is appropriate |
| Reporting | Overgeneralization | "A is faster" when A is faster *sometimes* |

The rest of this document is about recognizing and avoiding these failures.

---

# **Hash Table Fundamentals**

This document uses hash tables as the example domain. Here's what you need to know.

## What a Hash Table Does

A hash table stores key-value pairs and retrieves values by key in approximately constant time—O(1) on average.

```cpp
map.insert("alice", 42);   // Store
int x = map.find("alice"); // Retrieve: x = 42
```

The magic is the **hash function**: it converts any key into a number (the "hash"), which becomes an index into an array.

```
key "alice" -> hash function -> 7392847 -> index 47 (in a 1000-slot table)
```

## The Collision Problem

Two keys can hash to the same index. This is called a **collision**. Every hash table must handle collisions somehow.

### Strategy 1: Separate Chaining

Each slot holds a linked list of entries that hashed there:

```
Slot 47: [alice->42] -> [bob->17] -> [carol->99] -> null
```

To find "bob": hash to slot 47, walk the list until you find "bob".

**Pros:** Pointer stability (entries never move), no size limit on chains
**Cons:** Each entry is a separate heap allocation; following pointers causes cache misses

This is what `std::unordered_map` uses.

### Strategy 2: Open Addressing

All entries live in one array. On collision, probe forward to find an empty slot:

```
Insert "alice" (hashes to 47): Slot 47 is empty -> store there
Insert "bob" (hashes to 47):   Slot 47 is full -> try 48 -> empty -> store there
Insert "carol" (hashes to 47): Slots 47, 48 full -> try 49 -> empty -> store there
```

To find "bob": hash to 47, it's "alice" (not bob), probe to 48, found.

**Pros:** All data in one contiguous array (cache-friendly)
**Cons:** Deletion requires tombstones or shifting; pointers invalidate on rehash

## The Storage Question: Flat vs. Node

Open addressing has two variants:

**Flat storage:** Key-value pairs stored directly in the slot array.
- Maximum cache efficiency
- Pointers to values become invalid when table rehashes

**Node storage:** Slots store pointers to separately-allocated nodes.
- One extra indirection per access
- Pointers to values remain stable across rehash

This is the fundamental tradeoff StableHashMap navigates.

---

# **Swiss Tables: The Modern Approach**

In 2017, Google engineers introduced "Swiss Tables"—a hash table design that uses SIMD instructions to check multiple slots in parallel. This revolutionized hash table performance.

## The Control Byte Array

Swiss Tables organize slots into **groups** of 16. Each group has a 16-byte **control array** where each byte describes the corresponding slot:

| Control Value | Meaning |
|---------------|---------|
| 0x00 - 0x7F | Occupied; value is top 7 bits of hash ("H2 fingerprint") |
| 0x80 | Empty slot |
| 0xFE | Deleted (tombstone) |

The H2 fingerprint acts as a Bloom filter. With 128 possible values, a random occupied slot has only a 1/128 ≈ 0.78% chance of matching your search key's H2. This means ~99% of occupied slots are eliminated without comparing keys.

## The SIMD Comparison

The key insight: load 16 control bytes into a SIMD register, broadcast your target H2, compare all 16 in one instruction:

```cpp
// Load 16 control bytes
__m128i ctrl = _mm_load_si128(&control[group]);

// Broadcast H2 to all 16 positions
__m128i h2_vec = _mm_set1_epi8(target_h2);

// Compare all 16 in ONE instruction
__m128i matches = _mm_cmpeq_epi8(ctrl, h2_vec);

// Extract match positions
uint32_t mask = _mm_movemask_epi8(matches);
```

What would take 16 serial comparisons now takes 1 SIMD operation.

## Why This Matters

**Miss lookups benefit most.** Traditional probing must check each occupied slot until finding empty. Swiss Tables detect empty slots in the same SIMD pass—often terminating without comparing any keys.

**StableHashMap uses Swiss Tables.** It implements the SIMD control byte probing with AVX2 instructions, achieving lookup speeds competitive with `absl::flat_hash_map`.

---

# **The Competitor Landscape**

When benchmarking StableHashMap, you'll compare against other hash table implementations. Each has different design goals. Understanding these is essential for interpreting results correctly.

## std::unordered_map (The Standard)

**Design:** Separate chaining with linked list buckets. Each entry is a separate heap allocation.

**Optimizes for:**
- Pointer/iterator stability (addresses remain valid across mutations)
- Standard compliance and portability
- Correctness over performance

**Weaknesses:**
- Per-entry heap allocation (slow insert, memory overhead)
- Pointer chasing (cache-unfriendly lookups)
- No SIMD acceleration

**Expected results vs StableHashMap:**

| Operation | StableHashMap | std::unordered_map | Why |
|-----------|---------------|-------------------|-----|
| Insert (Block) | **4.8x faster** | Slower | Block allocation vs per-node malloc |
| Find | **3.3x faster** | Slower | SIMD probing vs chain traversal |
| Miss | **4x faster** | Slower | SIMD empty detection |
| Erase (Block) | **5.4x faster** | Slower | Block deallocation vs free() |

**Interpretation:** StableHashMap should beat `std::unordered_map` on virtually everything. Both provide pointer stability, but StableHashMap adds SIMD acceleration and optional block allocation.

---

## absl::flat_hash_map (Flat Swiss Table)

**Design:** Swiss Table with flat storage—key-value pairs stored directly in slots.

**Optimizes for:**
- Maximum lookup speed
- Memory efficiency
- Cache locality

**Weaknesses:**
- Pointers to values invalidate on rehash
- External dependency (Abseil library)

**Expected results vs StableHashMap:**

| Operation | StableHashMap | absl::flat_hash_map | Why |
|-----------|---------------|---------------------|-----|
| Find | Slower (0.7-0.8x) | **Faster** | No node indirection |
| Insert | Similar | Similar | Both Swiss Table |
| Pointer stability | **Yes** | No | Node vs flat storage |

**Interpretation:**

- **Losing on lookup is expected.** Flat storage avoids one pointer dereference. This is ~2-5ns overhead per lookup.
- **This is not a benchmark failure.** It's the cost of pointer stability.
- **If you don't need pointer stability, use flat tables.** They're faster.

---

## absl::node_hash_map (Node Swiss Table)

**Design:** Swiss Table with node storage—slots store pointers to separately-allocated nodes.

**Optimizes for:**
- Lookup speed with pointer stability
- Google-scale production workloads

**Expected results vs StableHashMap:**

| Operation | StableHashMap | absl::node_hash_map | Why |
|-----------|---------------|---------------------|-----|
| Find | Similar (0.8-1.0x) | Similar | Both node-based Swiss Table |
| Insert | Similar | Similar | Both allocate nodes |
| Erase | Similar | Similar | Both tombstone-based |

**Interpretation:**

- **Similar performance is expected.** Both are node-based Swiss Tables.
- **StableHashMap's advantage is zero dependencies.** If Abseil is already in your project, `absl::node_hash_map` is excellent.
- **StableHashMap adds the Block allocator.** For insert-heavy workloads, this provides 2-4x speedup.

---

## boost::unordered_node_map (Boost Swiss Table)

**Design:** Swiss Table with node storage, highly optimized.

**Expected results vs StableHashMap:**

| Operation | StableHashMap | boost::unordered_node_map | Why |
|-----------|---------------|--------------------------|-----|
| Find | Similar | Similar | Both node-based |
| Miss | Slower (~0.5x) | **Faster** | Different group layout |
| Insert | Similar | Similar | Both allocate nodes |

**Interpretation:**

- **boost has faster miss detection.** ~2x faster for miss-heavy workloads.
- **This is a real difference**, not a benchmark artifact. boost's 15-slot groups and probing strategy examine fewer candidates per miss.
- **For miss-heavy workloads (caches, filters), consider boost.**

---

## Quick Reference: Expected Results

Use this table to sanity-check your benchmark results:

| Competitor | Insert | Find | Miss | Erase |
|------------|--------|------|------|-------|
| std::unordered_map | StableHashMap 4.8x | StableHashMap 3.3x | StableHashMap 4x | StableHashMap 5.4x |
| absl::flat_hash_map | Similar | absl 1.2-1.4x | absl 1.5x | Similar |
| absl::node_hash_map | Similar | Similar | Similar | Similar |
| boost::unordered_node_map | Similar | Similar | boost 2x | Similar |

*StableHashMap with Block allocator compared.*

**Red flags in your benchmark:**

| Result | Likely Problem |
|--------|----------------|
| std::unordered_map beats StableHashMap | Benchmark error, or N is tiny |
| StableHashMap beats absl::flat on lookup | Benchmark error (flat should win) |
| StableHashMap matches boost on miss | Benchmark error (boost should win on miss) |
| Any result with >5x difference on similar operations | Verify you're measuring the same semantics |

---

## What "Winning" and "Losing" Actually Mean

**StableHashMap's value proposition is not "fastest on every metric."**

It's the combination of:
1. **SIMD acceleration** (3-5x faster than std::unordered_map)
2. **Pointer stability** (unlike flat tables)
3. **Block allocator option** (2-4x faster insert/erase)
4. **Built-in hash mixer** (protects against weak hashes)
5. **Zero dependencies** (unlike Abseil/Boost)

If your benchmark shows StableHashMap beating flat tables on lookup, something is wrong. If it shows boost beating StableHashMap on miss, that's expected.

---

# **Statistics You Need to Know**

Benchmarks produce numbers. Statistics help you interpret them. But a statistic is only useful if you know what it *means*—what it tells you about your system, and when to worry.

## Mean (Average)

Add up all values, divide by count.

```
Samples: [30, 32, 31, 35, 180]
Mean: (30 + 32 + 31 + 35 + 180) / 5 = 61.6 ns
```

**What it tells you:** The "expected value" if you assume all outcomes are equally likely. Useful for capacity planning—if you know the mean and the request rate, you can estimate total CPU time.

**The trap:** One outlier (180) pulled the mean far above the typical value (~32). The mean says "61.6 ns" but 80% of your samples were under 35 ns. The mean misrepresents typical behavior.

**When to use it:** Throughput calculations, total resource consumption, batch processing where you care about aggregate time rather than individual operations.

**When to avoid it:** Latency-sensitive systems where outliers matter but shouldn't dominate the "typical" picture.

## Median

Sort the values, take the middle one.

```
Sorted: [30, 31, 32, 35, 180]
Median: 32 ns (the middle value)
```

**What it tells you:** The "typical" experience—half of operations are faster, half are slower. The median answers: "What will *most* operations look like?"

**Why it's robust:** The outlier (180) doesn't affect the median at all. You could change it to 10,000 and the median would still be 32. This makes median stable across noisy benchmark runs.

**When to use it:** Latency measurements, user-facing response times, anywhere you want to describe "normal" performance.

| Situation | Use Mean | Use Median |
|-----------|----------|------------|
| "How much total CPU time?" | YES | |
| "How fast is a typical lookup?" | | YES |
| "Can we handle 10K requests/sec?" | YES | |
| "What latency will users see?" | | YES |

**Rule of thumb:** Latency -> median. Throughput -> mean.

## Percentiles: P50, P99, P999

Percentiles tell you "X% of values are below this number."

```
1000 samples, sorted:
P50 (median): sample #500 = 32 ns    <--  Typical case
P90:          sample #900 = 45 ns    <--  10% are slower than this
P99:          sample #990 = 180 ns   <--  1% are slower than this
P999:         sample #999 = 850 ns   <--  0.1% are slower than this
```

**What they tell you:**

| Percentile | Question It Answers |
|------------|---------------------|
| P50 | "What's typical?" |
| P90 | "What's the bad-but-common case?" |
| P99 | "What's the worst case users regularly see?" |
| P999 | "What's the rare catastrophic case?" |

**Good vs bad values—the ratio test:**

The ratio between P99 and P50 reveals system stability:

| P99 / P50 Ratio | Interpretation |
|-----------------|----------------|
| < 2x | Excellent. Tight distribution, predictable performance |
| 2–5x | Normal. Some tail latency, typical for real systems |
| 5–10x | Concerning. Something occasionally goes very wrong |
| > 10x | Problematic. Investigate—you have outlier events |

**Example interpretation:**
```
P50: 32 ns, P99: 180 ns -> Ratio: 5.6x
```
This is borderline concerning. Most operations are fast, but 1 in 100 is 5x slower. If this is a hash table benchmark, something is occasionally causing cache misses or collisions. Worth investigating.

**Why P99 matters:** In a system handling 10,000 requests/second, P99 latency affects 100 requests *every second*. That's not rare—that's constant.

## Variance and Standard Deviation

**Variance** measures how spread out the data is. **Standard deviation** (stddev) is the square root, in the same units as the data.

```
Tight:  [31, 32, 31, 33, 32] -> stddev ~ 0.8 ns
Loose:  [10, 50, 30, 90, 20] -> stddev ~ 30 ns
```

**What it tells you:** How *consistent* your measurements are. Low stddev means the operation takes about the same time every time. High stddev means something is varying—maybe the operation itself, maybe external interference.

**Good vs bad—the coefficient of variation:**

Raw stddev is hard to interpret. Is 5 ns "high"? Depends on the mean. Use the coefficient of variation (CV):

```
CV = stddev / mean
```

| CV | Interpretation |
|----|----------------|
| < 5% | Excellent. Very stable benchmark |
| 5–15% | Good. Normal measurement noise |
| 15–30% | Acceptable but investigate |
| > 30% | Poor. Benchmark is unstable, results unreliable |

**Example:**
```
Mean: 35 ns, Stddev: 8 ns -> CV = 23%
```
This benchmark is on the edge. You can probably trust the median, but the mean is being pulled around by noise. Consider more iterations or a quieter test environment.

**What high variance signals:**
- External interference (other processes, interrupts)
- Thermal throttling (CPU slowing down)
- Cache effects (some runs hit cache, some miss)
- Measurement noise (timer resolution)
- Actual variability in the operation

## Confidence Intervals (CI)

A 95% confidence interval says: "If I repeated this experiment many times, 95% of the computed intervals would contain the true mean."

```
Mean: 35.2 ns, CI95: +/- 2.1 ns
Interpretation: The true average is probably between 33.1 and 37.3 ns
```

**What it tells you:** How much you should trust your mean estimate given your sample size. Narrow CI = high confidence. Wide CI = need more samples.

**Good vs bad CI—relative to effect size:**

If you're comparing two implementations:
```
Implementation A: 35.2 ns +/- 2.1 ns
Implementation B: 38.5 ns +/- 2.3 ns
```

The CIs don't overlap, so you can confidently say A is faster. But:

```
Implementation A: 35.2 ns +/- 4.0 ns
Implementation B: 36.1 ns +/- 3.8 ns
```

The CIs overlap heavily. You *cannot* conclude A is faster—the difference might just be noise.

**Rule of thumb:** If the difference between means is smaller than the CI width, you can't trust the comparison.

**Critical warning:** CI applies to the *mean*. Reporting CI alongside *median* is statistically incoherent—a common mistake. If you report median, use interquartile range (IQR) instead:

```
Median: 32 ns, IQR: 30–35 ns (25th to 75th percentile)
```

## Putting It Together: A Real Example

```
Benchmark: HashMap lookup (1M operations, 50 runs)

Results:
  Mean:   38.4 ns    (use for throughput calculations)
  Median: 32.1 ns    (use for "typical latency")
  Stddev: 12.3 ns    (CV = 32%—high variance, investigate)
  P99:    89.2 ns    (P99/P50 = 2.8x—acceptable tail)
  CI95:   +/- 3.4 ns   (mean is 38.4 +/- 3.4, so 35–42 ns range)
```

**Interpretation:**
- Typical lookup: ~32 ns (median)
- But high variance (CV 32%) suggests inconsistent results
- Tail isn't terrible (P99 only 2.8x median)
- The mean is higher than median -> right-skewed distribution (expected for latency)
- Need to either accept the noise or find a quieter test environment

**Decision:** Report median (32.1 ns) as the headline number. Mention P99 (89.2 ns) for tail-sensitive users. Note the high variance as a caveat on precision.

---

# **The Measurement Problem**

Even with perfect understanding of hash tables and statistics, measurement itself is hard. This section explains what goes wrong and—critically—**how to detect it**.

## Time Is Not Constant

Modern CPUs don't run at fixed speed:

- **Turbo boost:** CPU speeds up when cool, slows down when hot
- **Power saving:** CPU slows down when idle
- **Thermal throttling:** CPU slows down when overheating

A benchmark that runs 10 seconds may see different CPU speeds at the beginning vs. end.

**How to detect it:**

Compare your first 10 samples to your last 10 samples:

```cpp
// After collecting all samples:
double early_median = median(samples[0..9]);
double late_median = median(samples[N-10..N-1]);
double drift = (late_median - early_median) / early_median;

if (abs(drift) > 0.10) {
    // More than 10% drift: thermal effects detected
    std::cerr << "WARNING: " << drift*100 << "% drift detected\n";
}
```

| Drift | Interpretation | Action |
|-------|----------------|--------|
| < 5% | Normal | Results trustworthy |
| 5-10% | Mild thermal decay | Acceptable, note in report |
| 10-20% | Significant decay | Shorten benchmark or add cooling pauses |
| > 20% | Severe | Results unreliable, fix environment |

**Fixes:**
- Shorten individual runs (thermal doesn't accumulate)
- Add 1-second pauses between runs (let CPU cool)
- Pin to efficiency cores (no turbo, consistent speed)
- Monitor CPU frequency during benchmark if possible

## Caches Change Everything

The first time you access data, it's fetched from RAM (~100 ns). The second time, it's in cache (~1 ns). Same operation, 100x different timing.

```cpp
// First iteration: cold cache, slow
for (int i = 0; i < N; ++i) sum += data[i];  // ~100 ns/element

// Second iteration: warm cache, fast
for (int i = 0; i < N; ++i) sum += data[i];  // ~1 ns/element
```

Which timing is "correct"? Depends on your real workload.

**How to detect it:**

The classic symptom: your first sample is 10-100x slower than subsequent samples.

```
Run 1: 1,250 ns/op    <--  Cache cold
Run 2: 35 ns/op       <--  Cache warm
Run 3: 34 ns/op
Run 4: 36 ns/op
...
```

**How many warmup iterations?**

| Data Size | Warmup Iterations Needed |
|-----------|-------------------------|
| Fits in L1 (< 32 KB) | 1-2 |
| Fits in L2 (< 256 KB) | 2-3 |
| Fits in L3 (< 8 MB) | 3-5 |
| Exceeds L3 | 5-10 (may never stabilize) |

**Rule of thumb:** Run until 3 consecutive samples are within 10% of each other. Discard everything before that point.

**Which to measure—cold or warm?**

| Your Real Workload | Measure |
|-------------------|---------|
| Request handler (same data repeatedly) | Warm cache |
| Batch processing (streaming through data) | Cold cache |
| Mixed / Unknown | Report both |

If you report only warm-cache numbers but production is cold-cache, you've lied by omission.

## The Compiler Fights You

Compilers optimize aggressively. If a computation has no observable effect, the compiler may delete it:

```cpp
for (int i = 0; i < N; ++i) {
    map.find(i);  // Result unused -> compiler might delete this
}
```

You think you're timing 1 million lookups. The compiler reduced it to zero. Your "35 ns/op" is actually "0 ops happened."

**How to detect it:**

Suspiciously fast results. If your "1M lookups" complete in 1 microsecond, the compiler deleted them.

| Result | Likely Reality |
|--------|---------------|
| < 1 ns/op for non-trivial operation | Dead code eliminated |
| 0 ns/op | Definitely eliminated |
| Suspiciously round numbers | Loop unrolled away |

**Fixes:**

Use `benchmark::DoNotOptimize()` or volatile sinks:

```cpp
volatile int sink = 0;
for (int i = 0; i < N; ++i) {
    if (auto* v = map.find(i)) sink += *v;
}
```

Or with Google Benchmark:

```cpp
for (int i = 0; i < N; ++i) {
    auto* v = map.find(i);
    benchmark::DoNotOptimize(v);
}
```

**Verification:** Check the assembly. If your loop body is empty or trivial, the compiler won.

## Other Processes Interfere

Your benchmark shares the CPU with the operating system, background services, and other applications. Any of them can steal cycles, flush caches, or cause page faults.

**How to detect it:**

Look at your variance. High CV (coefficient of variation) often means external interference:

| CV | Likely Cause |
|----|--------------|
| < 5% | Clean environment |
| 5-15% | Normal system noise |
| 15-30% | Background processes or mild interference |
| > 30% | Major interference, results questionable |

Also look for **bimodal distributions**—two clusters of results:

```
Samples: 32, 34, 31, 35, 180, 33, 32, 195, 34, 31
         ^^^^^^^^^^^^^^^^^    ^^^^^^^^^
         Normal cluster        Interference cluster
```

If you see this, something interrupted some runs but not others.

**Fixes:**

| Problem | Fix |
|---------|-----|
| Background apps | Close browsers, Slack, email, IDEs |
| System services | Disable antivirus scanning temporarily |
| Scheduler noise | Pin benchmark to specific CPU cores |
| Power management | Set CPU governor to "performance" |
| Virtualization | Run on bare metal if possible |
| Network interrupts | Disable network interfaces |

**Minimum viable quiet:**
1. Close all applications
2. Disable network
3. Run 50+ iterations
4. Report median (robust to occasional interference)

## How Many Samples Is Enough?

More samples = more stable statistics, but diminishing returns.

| Goal | Minimum Samples | Recommended |
|------|-----------------|-------------|
| Quick sanity check | 10 | 20 |
| Publishable results | 30 | 50 |
| High-precision comparison | 50 | 100+ |
| Detecting 5% difference | 100 | 200+ |

**When to stop:** If your median hasn't changed by more than 2% over the last 20 samples, you have enough.

---

# **Summary: What You Now Know**

Before proceeding to Part I, you should understand:

1. **Benchmarks** are controlled experiments with setup, measurement, and reporting phases
2. **Hash tables** use hash functions to convert keys to array indices
3. **Collisions** occur when multiple keys hash to the same slot
4. **Separate chaining** uses linked lists (pointer chasing, cache-unfriendly)
5. **Open addressing** uses probe sequences (cache-friendly, but pointer stability varies)
6. **Swiss Tables** use SIMD to compare 16 slots in parallel
7. **Flat storage** maximizes cache efficiency but invalidates pointers on rehash
8. **Node storage** adds one indirection but preserves pointer stability
9. **std::unordered_map** provides stability but no SIMD; StableHashMap should beat it
10. **absl::flat_hash_map** is faster but lacks pointer stability
11. **boost::unordered_node_map** has faster miss detection than StableHashMap
12. **Mean** is affected by outliers; **median** is robust; use CV to assess variance
13. **P99/P50 ratio** tells you about tail behavior; >5x is concerning
14. **Confidence intervals** apply to means, not medians
15. **Measurement** is hard: detect thermal drift, count warmup iterations, verify code isn't eliminated

With this foundation, the traps in Part I will make sense.

---

# **PART I — THE TRAPS**

Every trap in this section was encountered during StableHashMap development. Each produced misleading results that initially seemed correct.

---

# **CHAPTER 1 — The Stability Assumption**

## The Original Benchmark

```cpp
// THE TRAP: Comparing apples to oranges
void bench_find(benchmark::State& state) {
    absl::flat_hash_map<int, Data> flat_map;
    fat_p::StableHashMap<int, Data> stable_map;
    
    for (int i = 0; i < N; ++i) {
        flat_map[i] = data;
        stable_map[i] = data;
    }
    
    for (auto _ : state) {
        for (int i = 0; i < N; ++i) {
            benchmark::DoNotOptimize(flat_map.find(i));  // or stable_map
        }
    }
}
```

This benchmark shows `absl::flat_hash_map` winning by 20-40%. The conclusion: "StableHashMap is slower."

**The problem:** The benchmark ignores *why* StableHashMap exists.

## What the Benchmark Missed

StableHashMap provides pointer stability. Flat tables don't. This is a correctness requirement for certain code patterns:

```cpp
// This code REQUIRES pointer stability
absl::flat_hash_map<int, Data> map;
map[1] = data1;
Data* ptr = &map[1];  // Get pointer

map[2] = data2;  // Might rehash!

ptr->process();  // UNDEFINED BEHAVIOR with flat tables
                 // WORKS with StableHashMap
```

**The correct comparison:** StableHashMap vs `absl::node_hash_map` or `boost::unordered_node_map`—implementations that also provide pointer stability.

## The Corrected Benchmark

```cpp
// Compare node-based implementations (fair comparison)
void bench_find_node_based(benchmark::State& state) {
    absl::node_hash_map<int, Data> absl_node;
    boost::unordered_node_map<int, Data> boost_node;
    fat_p::StableHashMap<int, Data> stable_map;
    
    // ... measure each
}
```

**Results when comparing correctly:**

| Map | Find (ns) |
|-----|-----------|
| boost::unordered_node_map | 11 |
| StableHashMap | 13 |
| absl::node_hash_map | 16 |

StableHashMap is competitive with other node-based Swiss Tables, not "slow."

---

# **CHAPTER 2 — API Equivalence Is Not Semantic Equivalence**

## The Naive Comparison

```cpp
// THE TRAP: Same API, different semantics
map1.emplace(key, value);
map2.emplace(key, value);
```

Both calls look identical. But:
- `std::unordered_map::emplace` **ignores** duplicate keys
- `StableHashMap::emplace` **overwrites** duplicate keys

A benchmark that inserts duplicates will measure different amounts of work.

## The Fix: Semantic Wrappers

```cpp
// Wrapper that enforces identical semantics
template <typename Map>
void insert_no_overwrite(Map& map, const Key& k, const Value& v) {
    if constexpr (has_try_emplace<Map>) {
        map.try_emplace(k, v);  // No overwrite
    } else {
        if (!map.contains(k)) {
            map.insert(k, v);
        }
    }
}
```

Now the benchmark measures the same logical operation regardless of implementation.

---

# **CHAPTER 3 — Allocation Boundaries Leak**

## The Hidden Allocation

```cpp
// THE TRAP: Allocation during measurement
void bench_insert(benchmark::State& state) {
    for (auto _ : state) {
        Map map;
        map.reserve(N);
        for (int i = 0; i < N; ++i) {
            map.insert(i, value);
        }
    }
}
```

With `std::unordered_map`, `reserve()` pre-allocates buckets but **not nodes**. Each `insert()` still calls `malloc()`.

With StableHashMap's Block allocator, `reserve()` pre-allocates node blocks. Insertions don't call `malloc()`.

**The result:** StableHashMap appears 4.8x faster on "insert." But some of that difference is allocation, not hashing.

## The Fix: Measure What You Mean

Option 1: **Exclude allocation from both**

```cpp
void bench_insert_no_alloc(benchmark::State& state) {
    Map map;
    map.reserve(N);
    // Pre-warm to force any lazy allocation
    for (int i = 0; i < N; ++i) map.insert(i, value);
    map.clear();
    
    for (auto _ : state) {
        for (int i = 0; i < N; ++i) {
            map.insert(i, value);
        }
        state.PauseTiming();
        map.clear();
        state.ResumeTiming();
    }
}
```

Option 2: **Label honestly**

```
Insert (including allocation): StableHashMap 4.8x faster
Insert (excluding allocation): StableHashMap 1.5x faster
```

Both are valid; the label determines which is informative.

---

# **CHAPTER 4 — Statistics Can Obscure Reality**

## The Confidence Interval Trap

```
StableHashMap find: 13.2 ns ± 0.3 (95% CI)
absl::node find:    12.8 ns ± 0.4 (95% CI)
```

These confidence intervals overlap. Is there a real difference?

**The problem:** CI assumes normal distribution. Latency distributions are often skewed.

**The fix:** Use median + IQR for latency:

```
StableHashMap find: 13 ns (IQR: 2 ns)
absl::node find:    11 ns (IQR: 2 ns)
```

Or report the full distribution:

| Map | P50 | P90 | P99 |
|-----|-----|-----|-----|
| StableHashMap | 13 | 15 | 22 |
| absl::node | 11 | 14 | 28 |

Now you can see that absl is faster at median but StableHashMap has better tail behavior.

---

# **PART II — THE FIXES**

---

# **CHAPTER 5 — Pointer Stability Verification**

## The Claim

> "Pointers to values remain valid across insertions and rehashes."

This is StableHashMap's core guarantee. It must be tested, not assumed.

## The Verification Test

```cpp
TEST(Stability, PointersRemainValidAcrossRehash) {
    fat_p::StableHashMap<int, int> map;
    
    // Insert initial elements
    for (int i = 0; i < 1000; ++i) {
        map[i] = i * 100;
    }
    
    // Capture pointers
    std::vector<int*> ptrs;
    for (int i = 0; i < 1000; ++i) {
        ptrs.push_back(map.find(i));
        ASSERT_NE(ptrs.back(), nullptr);
    }
    
    // Force multiple rehashes
    for (int i = 1000; i < 100000; ++i) {
        map[i] = i;
    }
    
    // Verify all original pointers still valid
    for (int i = 0; i < 1000; ++i) {
        EXPECT_EQ(*ptrs[i], i * 100)
            << "Pointer to key " << i << " invalidated after rehash";
    }
}
```

## Comparing Against Flat Tables

```cpp
TEST(Stability, FlatTablesInvalidatePointers) {
    absl::flat_hash_map<int, int> map;
    map[1] = 100;
    int* ptr = &map[1];
    int original = *ptr;
    
    // Force rehash
    for (int i = 2; i < 10000; ++i) {
        map[i] = i;
    }
    
    // This WILL fail (undefined behavior, but usually detectable)
    // Don't actually run this in production tests
    // Just document that flat tables lack this guarantee
}
```

---

# **CHAPTER 6 — Semantic Alignment**

## The emplace() Difference

StableHashMap's `emplace()` overwrites existing keys. std::unordered_map's doesn't.

For fair benchmarks, align semantics:

```cpp
// For "insert if missing" semantics
template <typename Map, typename K, typename V>
void insert_if_missing(Map& map, K&& k, V&& v) {
    if constexpr (std::is_same_v<Map, fat_p::StableHashMap<...>>) {
        map.try_emplace(std::forward<K>(k), std::forward<V>(v));
    } else {
        map.emplace(std::forward<K>(k), std::forward<V>(v));
    }
}

// For "upsert" semantics
template <typename Map, typename K, typename V>
void upsert(Map& map, K&& k, V&& v) {
    if constexpr (std::is_same_v<Map, fat_p::StableHashMap<...>>) {
        map.emplace(std::forward<K>(k), std::forward<V>(v));
    } else {
        map.insert_or_assign(std::forward<K>(k), std::forward<V>(v));
    }
}
```

---

# **CHAPTER 7 — Honest Labels Over Perfect Code**

## The Label Review Process

Before publishing any benchmark, review each claim:

| Claim | Question | Action |
|-------|----------|--------|
| "No allocation" | Does reserve() prevent all allocation? | Verify per-implementation |
| "Random access" | Is the RNG seeded identically? | Check initialization |
| "Cache cold" | Is cache actually flushed? | Verify or relabel |
| "N operations" | Does N mean iterations or elements? | Clarify |

## Examples of Honest Relabeling

**Before:** "Insert (no allocation)"
**After:** "Amortized build from empty (allocation behavior varies by implementation)"

**Before:** "Random lookup"
**After:** "Pseudo-random lookup (fixed seed, reproducible sequence)"

**Before:** "Cache cold"
**After:** "First access after clear (cache state uncontrolled)"

**Before:** "Pointer stable"
**After:** "Value addresses stable across insertions and rehashes (not across erase of that key)"

## Why Honesty Beats Perfection

A benchmark with honest limitations:
- Can be evaluated by reviewers
- Won't be invalidated by edge case discovery
- Communicates what was actually measured

A benchmark claiming perfection:
- Invites adversarial scrutiny
- Fails when assumptions are violated
- Damages credibility of all results

**Key principle:**

> The goal is *defensible* results, not *impressive* results.

---

# **CHAPTER 8 — Platform Variance as Signal**

## The Uncomfortable Discovery

The same benchmark on Windows and Linux produced different results:

| Configuration | StableHashMap | std::unordered_map |
|--------------|---------------|-------------------|
| Windows + std::hash | Wins by 2.5x | Baseline |
| Linux + std::hash | Wins by 1.8x | Baseline |
| Windows + SplitMix64 | Wins by 3.2x | Baseline |
| Linux + SplitMix64 | Wins by 1.6x | Baseline |

Why does hash function choice matter more on Windows?

## The Root Cause

MSVC's `std::hash<int64_t>` is the identity function—no bit mixing. This causes clustering in open-addressing tables. GCC/libstdc++ does reasonable mixing by default.

SplitMix64 provides consistent mixing across platforms. On Windows, it's a large improvement. On Linux, it's a small regression (extra computation, marginal benefit).

## The Response: Policy, Not Uniformity

Instead of:
> "Always use SplitMix64"

The response was:
> "Expose hash policy; document platform characteristics"

Benchmarks informed configuration guidance:

| Platform | Recommended Hash | Rationale |
|----------|-----------------|-----------|
| Windows/MSVC | SplitMix64 or similar | std::hash is identity |
| Linux/GCC | std::hash acceptable | Already mixes well |
| Cross-platform | Use built-in mixer | StableHashMap applies mixer by default |

## Why Platform Variance Matters

Many benchmark suites quietly ignore:
- Windows scheduler variability
- Turbo boost decay over long runs
- Hybrid core scheduling (P-cores vs E-cores)
- Thermal throttling

StableHashMap benchmarks treated platform variance as *signal*:
- If performance varies by platform, that's useful information
- If a claim doesn't survive Windows testing, it's not a real claim
- If results depend on hash quality, document the dependency

**Key principle:**

> Platform variance reveals assumptions. Hidden assumptions become production bugs.

---

# **PART III — THE VERIFICATION SUITE**

Benchmarks measure performance. Tests verify correctness. The claims that StableHashMap makes—pointer stability, SIMD acceleration, block allocator efficiency—require explicit verification beyond timing measurements.

**Key principle:**

> *The most valuable benchmarks are those that can fail in CI.*

A benchmark that only produces numbers for humans to read is documentation. A benchmark with assertions that break the build when invariants are violated is *engineering*.

---

# **CHAPTER 9 — White-Box Stability Tests**

## The Testing Problem

StableHashMap claims: "Pointers to values remain stable across insertions and rehashes."

How do you *prove* this, not just measure symptoms?

**Black-box testing:** Store pointers, perform operations, check if pointers still work.
**White-box testing:** Inspect internal state, verify that node addresses haven't changed.

Black-box testing catches crashes but can miss subtle corruption. White-box testing provides direct evidence.

## The Test Infrastructure

A test accessor class provides internal inspection without polluting the public API:

```cpp
// Friend class for test access
template <typename MapType>
class StableHashMapTester {
public:
    // Get the address of a node's value storage
    static void* get_node_address(MapType& map, 
                                  const typename MapType::key_type& key) {
        auto* ptr = map.find(key);
        return static_cast<void*>(ptr);
    }
    
    // Count allocated nodes (for block allocator verification)
    static size_t count_allocated_nodes(const MapType& map) {
        // Implementation depends on allocator policy
        return map.node_count_;
    }
    
    // Get control byte at position (for SIMD verification)
    static uint8_t get_control_byte(const MapType& map, size_t pos) {
        return map.control_[pos];
    }
    
    // Check if position is occupied
    static bool is_occupied(const MapType& map, size_t pos) {
        return (map.control_[pos] & 0x80) == 0;
    }
    
    // Get H2 fingerprint at position
    static uint8_t get_h2(const MapType& map, size_t pos) {
        uint8_t ctrl = map.control_[pos];
        return (ctrl & 0x80) == 0 ? ctrl : 0xFF;
    }
};
```

Add the friend declaration to StableHashMap:

```cpp
template <typename K, typename V, typename P>
friend class StableHashMapTester;
```

## The Pointer Stability Verification Test

```cpp
TEST(Stability, PointersRemainValidAcrossRehash) {
    using Map = fat_p::StableHashMap<int, std::string>;
    using Tester = StableHashMapTester<Map>;
    
    Map map;
    
    // Phase 1: Insert initial elements and capture addresses
    std::vector<std::pair<int, void*>> addresses;
    for (int i = 0; i < 1000; ++i) {
        map[i] = "value_" + std::to_string(i);
        addresses.emplace_back(i, Tester::get_node_address(map, i));
    }
    
    size_t initial_capacity = map.capacity();
    
    // Phase 2: Force multiple rehashes
    for (int i = 1000; i < 100000; ++i) {
        map[i] = "value_" + std::to_string(i);
    }
    
    // Verify rehashes occurred
    ASSERT_GT(map.capacity(), initial_capacity * 4)
        << "Test invalid: rehashes didn't occur";
    
    // Phase 3: Verify all original addresses unchanged
    for (const auto& [key, original_addr] : addresses) {
        void* current_addr = Tester::get_node_address(map, key);
        EXPECT_EQ(current_addr, original_addr)
            << "Pointer to key " << key << " changed after rehash: "
            << original_addr << " -> " << current_addr;
    }
}
```

## The Erase Invalidation Test

```cpp
TEST(Stability, EraseInvalidatesOnlyErasedKey) {
    using Map = fat_p::StableHashMap<int, int>;
    
    Map map;
    for (int i = 0; i < 100; ++i) map[i] = i * 100;
    
    // Capture pointers to all values
    std::map<int, int*> ptrs;
    for (int i = 0; i < 100; ++i) {
        ptrs[i] = map.find(i);
        ASSERT_NE(ptrs[i], nullptr);
    }
    
    // Erase every other key
    for (int i = 0; i < 100; i += 2) {
        map.erase(i);
    }
    
    // Verify: erased keys return nullptr
    for (int i = 0; i < 100; i += 2) {
        EXPECT_EQ(map.find(i), nullptr)
            << "Erased key " << i << " should return nullptr";
    }
    
    // Verify: non-erased keys still have same pointer AND value
    for (int i = 1; i < 100; i += 2) {
        int* current = map.find(i);
        EXPECT_EQ(current, ptrs[i])
            << "Pointer to non-erased key " << i << " changed";
        EXPECT_EQ(*current, i * 100)
            << "Value at key " << i << " corrupted";
    }
}
```

---

# **CHAPTER 10 — SIMD Acceleration Verification**

## The Claim

> "SIMD probing checks 16 candidates per instruction."

This is an implementation claim that affects performance but isn't visible in timing alone.

## The Control Byte Verification Test

```cpp
TEST(SwissTable, ControlBytesCorrectlyPopulated) {
    using Map = fat_p::StableHashMap<int, int>;
    using Tester = StableHashMapTester<Map>;
    
    Map map;
    
    // Insert with known hash
    int key = 42;
    map[key] = 100;
    
    // Compute expected H2 (low 7 bits of hash)
    uint64_t hash = std::hash<int>{}(key);
    // Note: StableHashMap applies SplitMix64 mixer
    hash = splitmix64(hash);
    uint8_t expected_h2 = hash & 0x7F;
    
    // Find the key's position and verify control byte
    bool found = false;
    for (size_t i = 0; i < map.capacity(); ++i) {
        if (Tester::is_occupied(map, i)) {
            uint8_t actual_h2 = Tester::get_h2(map, i);
            if (actual_h2 == expected_h2) {
                // Verify this is actually our key
                // (H2 collision possible but unlikely)
                found = true;
                break;
            }
        }
    }
    
    EXPECT_TRUE(found)
        << "Control byte H2 mismatch for key " << key;
}
```

## The SIMD Speedup Benchmark

```cpp
void bench_find_hit(benchmark::State& state) {
    Map map;
    std::vector<int> keys = generate_random_keys(N);
    for (int k : keys) map[k] = k;
    
    std::mt19937 rng(42);
    std::shuffle(keys.begin(), keys.end(), rng);
    
    for (auto _ : state) {
        for (int k : keys) {
            benchmark::DoNotOptimize(map.find(k));
        }
    }
}

void bench_find_miss(benchmark::State& state) {
    Map map;
    for (int i = 0; i < N; ++i) map[i] = i;
    
    std::vector<int> miss_keys;
    for (int i = N; i < 2*N; ++i) miss_keys.push_back(i);
    
    for (auto _ : state) {
        for (int k : miss_keys) {
            benchmark::DoNotOptimize(map.find(k));
        }
    }
}
```

**Expected results vs std::unordered_map:**

| Operation | StableHashMap | std::unordered_map | Speedup |
|-----------|---------------|-------------------|---------|
| Find (hit) | 13 ns | 29 ns | 2.2x |
| Find (miss) | 9 ns | 36 ns | 4x |

The miss speedup is larger because SIMD detects empty slots without comparing keys.

## Interpreting Results

| Result | Interpretation |
|--------|---------------|
| Hit speedup 2-3x over std::unordered_map | SIMD working correctly |
| Miss speedup 3-5x over std::unordered_map | Empty slot detection working |
| Miss slower than hit | Unexpected—investigate |
| Similar to std::unordered_map | SIMD not engaging—check compilation flags |

---

# **CHAPTER 11 — Block Allocator Efficiency Tests**

## The Claim

> "Block allocation reduces malloc() calls from N to approximately N/1024."

## The Allocation Counting Test

```cpp
TEST(BlockAllocator, AllocationCountReduced) {
    // Custom allocator that counts allocations
    struct CountingAllocator {
        static inline size_t alloc_count = 0;
        static inline size_t dealloc_count = 0;
        
        void* allocate(size_t n) {
            ++alloc_count;
            return ::malloc(n);
        }
        
        void deallocate(void* p, size_t) {
            ++dealloc_count;
            ::free(p);
        }
        
        static void reset() {
            alloc_count = 0;
            dealloc_count = 0;
        }
    };
    
    const int N = 100000;
    
    // Test with block allocator
    CountingAllocator::reset();
    {
        fat_p::StableHashMap<int, int, 
            fat_p::BlockAllocatorPolicy,
            std::hash<int>,
            CountingAllocator> block_map;
        
        for (int i = 0; i < N; ++i) {
            block_map[i] = i;
        }
    }
    size_t block_allocs = CountingAllocator::alloc_count;
    
    // With block size ~1024, expect ~100 node allocations
    // Plus control array allocations (log2 rehashes)
    EXPECT_LT(block_allocs, 200)
        << "Block allocator made " << block_allocs 
        << " allocations for " << N << " elements (expected < 200)";
    
    // Compare to standard allocation (if available)
    // Standard would be ~N allocations
}
```

## The Block Allocator Benchmark

```cpp
void bench_insert_standard(benchmark::State& state) {
    for (auto _ : state) {
        fat_p::StableHashMap<int, Data> map;  // Standard allocation
        for (int i = 0; i < N; ++i) {
            map[i] = Data{i};
        }
    }
}

void bench_insert_block(benchmark::State& state) {
    for (auto _ : state) {
        fat_p::StableHashMap<int, Data, fat_p::BlockAllocatorPolicy> map;
        for (int i = 0; i < N; ++i) {
            map[i] = Data{i};
        }
    }
}
```

**Expected results:**

| N | Standard | Block | Improvement |
|---|----------|-------|-------------|
| 1K | 35 ns | 18 ns | 1.9x |
| 10K | 38 ns | 17 ns | 2.2x |
| 100K | 40 ns | 17 ns | 2.4x |
| 1M | 40 ns | 17 ns | 2.4x |

## The Erase Efficiency Test

Block allocation also improves erase by deferring deallocation:

```cpp
void bench_erase_standard(benchmark::State& state) {
    for (auto _ : state) {
        state.PauseTiming();
        fat_p::StableHashMap<int, Data> map;
        for (int i = 0; i < N; ++i) map[i] = Data{i};
        state.ResumeTiming();
        
        for (int i = 0; i < N; ++i) {
            map.erase(i);
        }
    }
}

void bench_erase_block(benchmark::State& state) {
    for (auto _ : state) {
        state.PauseTiming();
        fat_p::StableHashMap<int, Data, fat_p::BlockAllocatorPolicy> map;
        for (int i = 0; i < N; ++i) map[i] = Data{i};
        state.ResumeTiming();
        
        for (int i = 0; i < N; ++i) {
            map.erase(i);
        }
    }
}
```

**Expected improvement:** 4-5x for erase operations.

---

# **PART IV — PRINCIPLES**

---

# **APPENDIX A — The Benchmarking Checklist**

Use this checklist before publishing any performance claim.

## Before Writing Code

- [ ] Define the semantic operation being measured (not just API name)
- [ ] Identify which implementations will be compared
- [ ] Document pointer stability differences
- [ ] Document allocation behavior differences
- [ ] Plan for both hit and miss measurements

## During Implementation

- [ ] Use identical RNG seeds across implementations
- [ ] Use semantic wrappers if APIs differ
- [ ] Measure setup time separately from operation time
- [ ] Include warmup iterations (discarded)
- [ ] Run sufficient iterations for stable measurements

## Before Publishing

- [ ] Compare like with like (node vs node, flat vs flat)
- [ ] Review all claims against actual measurement
- [ ] Relabel any claim that overstates guarantees
- [ ] Include platform information (OS, compiler, hardware)
- [ ] Report variability (IQR or percentiles)
- [ ] Acknowledge known limitations

## For Long-Term Validity

- [ ] Document methodology, not just results
- [ ] Include reproduction instructions
- [ ] Version control benchmark code alongside component
- [ ] Re-run benchmarks after significant changes

---

# **APPENDIX B — Anti-Patterns Catalog**

## Stability Assumption Fallacy

**Pattern:** Compare node-based to flat-based, declare flat "better"
**Failure:** Ignores that pointer stability is a correctness requirement
**Fix:** Compare implementations with the same stability guarantee

## API Name Matching

**Pattern:** Use same API names across implementations
**Failure:** APIs with same names may have different semantics
**Fix:** Align by semantics, use wrappers if needed

## Allocation Boundary Assumption

**Pattern:** Assume `reserve()` prevents all allocation
**Failure:** Different implementations have different reserve semantics
**Fix:** Verify per-implementation, or label honestly

## Single-Platform Generalization

**Pattern:** Benchmark on one platform, claim general results
**Failure:** Platform differences affect results significantly
**Fix:** Test multiple platforms, report differences

## Favorable-Condition Selection

**Pattern:** Choose benchmark parameters that favor your implementation
**Failure:** Real workloads have different parameters
**Fix:** Include adversarial benchmarks, acknowledge limitations

---

*End of Case Study*

---

## Summary

This document doesn't just evaluate StableHashMap. It establishes a methodology for honestly evaluating hash tables.

The principles:

1. **Compare like with like.** Node-based vs node-based. Flat vs flat.
2. **Align semantics.** APIs with the same name can have different behavior.
3. **Label honestly.** If you measure allocation, say so.
4. **Report variance.** Median + IQR, or percentiles.
5. **Acknowledge limitations.** Where does your implementation lose?

The goal is never "my implementation wins." The goal is "my claims are defensible."

Defensible claims survive scrutiny. Defensible claims inform real decisions. Defensible claims are worth publishing.

---

*FAT-P Library Documentation — December 2025*
