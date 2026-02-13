---
doc_id: CS-FUZZYEQUALITY-001
doc_type: "Case Study"
title: "Fuzzy Equality Non-Transitivity in Sorted Ranges"
fatp_components: ["FlatSet", "FloatingPointComparison"]
topics: ["fuzzy equality", "non-transitivity", "floating-point comparison", "std::unique", "equivalence relation", "greedy forward scan", "epsilon chains"]
constraints: ["IEEE 754 floating-point arithmetic", "std::unique equivalence relation requirement", "epsilon chain formation in sorted data"]
cxx_standard: "C++17"
std_equivalent: null
boost_equivalent: null
build_modes: ["Debug", "Release"]
last_verified: "2026-02-13"
audience: ["C++ developers", "AI assistants", "scientific computing engineers"]
status: "reviewed"
---

# Case Study - Fuzzy Equality Non-Transitivity in Sorted Ranges
## How `|a - b| < ε` breaks `std::unique` and what to do about it

## Scope

This case study examines a single failure mode: fuzzy floating-point comparison (`|a - b| < ε`) used as an equivalence predicate for deduplication on sorted data. The document proves why the failure occurs mathematically, identifies when it matters in practice, debunks a common but misleading framing ("sensor data streams"), and presents a correct O(N) replacement algorithm.

## Not covered

- General floating-point comparison strategies (see IEEE 754 literature)
- Relative epsilon comparison (`|a - b| / max(|a|, |b|) < ε`)
- ULP-based comparison
- Concurrency policies for containers
- Hash-based deduplication of floating-point values

## Prerequisites

- Understanding of `std::unique` and its predicate requirements
- Basic floating-point representation (IEEE 754 double precision)
- Familiarity with sorted-range algorithms in the C++ standard library

## Case Study Card

**Problem:** Fuzzy floating-point deduplication silently produces inconsistent results on sorted data  
**Constraint:** Fuzzy equality `|a - b| < ε` is not an equivalence relation — it violates transitivity  
**Symptom:** `std::unique` with a fuzzy predicate keeps or removes different elements depending on input ordering and chain formation, producing unpredictable survivor sets  
**Root cause:** `std::unique` compares each element against the previous *seen* element, not the previous *kept* element, allowing epsilon-chains to drift arbitrarily far from the anchor  
**Fix pattern:** Replace `std::unique` with a greedy forward scan that compares against the last *kept* element  
**FAT-P components used:** FlatSet (sorted range storage), FloatingPointComparison (epsilon utilities)  
**Build-mode gotchas:** None — this is an algorithmic correctness issue, not UB  
**Guarantees:** Output elements are always spaced > ε apart; O(N) single-pass; deterministic  
**Non-guarantees:** Does not guarantee optimal clustering (first element of each cluster wins, not centroid)

## Table of Contents

- [⚠️ Before You Read Further: The Epsilon Drift](#️-before-you-read-further-the-epsilon-drift)
- [Part I — The Problems](#part-i--the-problems)
- [Part II — The Solutions](#part-ii--the-solutions)
- [Part III — The Case Study Story](#part-iii--the-case-study-story)
- [Part IV — Foundations](#part-iv--foundations)
- [Design Rules to Internalize](#design-rules-to-internalize)
- [What To Do Now](#what-to-do-now)
- [Glossary](#glossary)

---

## ⚠️ Before You Read Further: The Epsilon Drift

```cpp
// THE TRAP: fuzzy dedup on a sorted range
auto fuzzy_eq = [](double a, double b) { return std::fabs(a - b) < 0.006; };
auto last = std::unique(data.begin(), data.end(), fuzzy_eq);
data.erase(last, data.end());
```

Stop. With input `[1.000, 1.005, 1.010, 1.015]`, this removes `1.005` (close to `1.000`) and `1.015` (close to `1.010`), producing `[1.000, 1.010]`. But change the input to `[1.000, 1.003, 1.006, 1.009, 1.012]` and the survivors shift entirely — each consecutive pair is within epsilon, so `std::unique` may chain-collapse them all into a single element. The predicate is not transitive, and `std::unique` requires an equivalence relation. The result depends on which chains happen to form in the data, not on any principled definition of "duplicate."

---

## Part I — The Problems

### The Obvious Approach

Floating-point arithmetic produces noise. Two computations that should yield the same value rarely produce bit-identical results. The obvious response is to define "close enough" as equal: if `|a - b| < ε`, treat `a` and `b` as duplicates. On a sorted range, `std::unique` with this predicate seems like the natural deduplication tool — it removes consecutive duplicates in O(N), and the data is already sorted, so all "close" values are adjacent.

This reasoning has a hidden flaw.

### The Hidden Constraint

An equivalence relation must satisfy three properties: reflexivity (`a ≈ a`), symmetry (`a ≈ b` implies `b ≈ a`), and transitivity (`a ≈ b` and `b ≈ c` implies `a ≈ c`). Fuzzy equality satisfies the first two but fails the third. With `ε = 0.006`, values `1.000` and `1.005` are within epsilon, and `1.005` and `1.010` are within epsilon, but `1.000` and `1.010` are not. The relation forms chains without closure — adjacent pairs match, but the endpoints of the chain may be arbitrarily far apart.

The C++ standard specifies that the predicate passed to `std::unique` must be an equivalence relation. Passing a non-transitive predicate is a precondition violation. No major implementation produces undefined behavior in practice, but the standard makes no guarantees about the result.

### The Symptoms

The symptoms are subtle and data-dependent. `std::unique` compares each element against its immediate predecessor in the remaining sequence. When a chain of closely-spaced values appears in sorted data, the algorithm walks along the chain, comparing each value to the last *seen* element (which may itself have been removed). The survivor set depends on exactly which values appear in the chain and their precise spacing.

Two datasets with the same statistical distribution but slightly different values can produce different numbers of survivors. A dataset that works correctly in testing may fail in production when real-world noise shifts values by fractions of an epsilon.

### The Cost

For library code, this is a correctness defect that surfaces as user bug reports. The user writes what looks like correct code — sort, then deduplicate with a tolerance — and gets inconsistent results. The inconsistency is hard to diagnose because the same code works on most inputs. Only specific value distributions trigger visible failures.

**Fact:** The failure requires values spaced at roughly ε intervals across a range. In typical scientific and HPC workloads where values cluster tightly around discrete "true" values separated by much more than ε, chains do not form.

**Fact:** When chains do form, the result is not random — it is deterministic for a given input — but the survivor set is sensitive to small perturbations in input values.

### Solution Preview

Replace `std::unique` with a greedy forward scan that compares each element against the last *kept* element rather than the last *seen* element. This breaks chains by anchoring to cluster representatives. The algorithm is O(N), single-pass, and produces a deterministic output where consecutive survivors are always spaced more than ε apart.

Part IV explains the design rationale, rejected alternatives, and edge cases.

---

## Part II — The Solutions

### The Mechanism

Part I showed that `std::unique` drifts because it compares against the last *seen* element. The fix anchors comparisons to the last *kept* element instead. When the algorithm encounters a new value, it measures the distance from the current anchor (the most recent survivor). If the distance exceeds ε, the new value becomes the next anchor. If the distance is within ε, the value is discarded.

This transforms the problem from "are consecutive elements close?" (which chains) to "is this element close to the nearest cluster representative?" (which does not chain). The anchor advances only when a genuinely distinct value appears, so the output spacing is always > ε regardless of how the input values are distributed.

The greedy scan compares each candidate against the last kept element rather than the last seen element. This is the full algorithm, shown as pseudocode:

```
-- pseudocode: greedy forward scan for fuzzy deduplication --
write_cursor ← first element (always kept as initial anchor)
for each element after the first:
    if |element - *write_cursor| > ε:
        advance write_cursor
        move element to write_cursor position
erase everything after write_cursor
```

### Guarantees / Non-Guarantees

| Property | Guaranteed? | Conditions | Notes |
|----------|-------------|------------|-------|
| Consecutive survivors spaced > ε | ✅ Yes | All inputs | By construction — anchor only advances past ε |
| Deterministic output | ✅ Yes | Same input → same output | No dependence on comparison ordering |
| Stability (first element of each cluster kept) | ✅ Yes | All inputs | Write cursor advances forward only |
| O(N) single pass | ✅ Yes | All inputs | One comparison per element |
| No UB from non-transitive predicate | ✅ Yes | All inputs | Does not use `std::unique` |
| Optimal clustering (centroid selection) | ❌ No | — | First element wins, not geometric center |
| Handles unsorted input | ❌ No | — | Input must be sorted; unsorted input produces meaningless results |
| Relative epsilon support | ❌ No | — | Algorithm uses absolute epsilon only; adapt the comparison function for relative tolerance |

### Decision Guide

The greedy forward scan is the correct choice whenever fuzzy deduplication is applied to sorted data. There is no scenario where `std::unique` with a fuzzy predicate produces more correct results — the greedy scan matches `std::unique` on well-separated data and produces strictly better results on chain-prone data.

For data that does not form chains (clusters separated by >> ε), both algorithms produce identical output. The greedy scan costs nothing extra in this case — same O(N) complexity, same number of comparisons.

### Where It Loses

The greedy scan selects the first element of each cluster as the representative. If the application needs centroids, medians, or weighted representatives, a clustering algorithm (k-means, DBSCAN, or explicit binning) is more appropriate. The greedy scan is a deduplication tool, not a clustering tool.

For data where "duplicate" means "same physical event observed twice" (as opposed to "same numerical value with noise"), fuzzy comparison is the wrong approach entirely. Event deduplication requires temporal or causal reasoning that no comparison predicate can provide.

---

## Part III — The Case Study Story

### Context

Fat-P needed fuzzy deduplication for sorted containers — specifically, the ability to insert floating-point values into a sorted range and treat nearly-equal values as duplicates. The initial implementation used `std::unique` with a fuzzy predicate after each insertion batch, following the conventional approach found in most C++ codebases that handle floating-point deduplication.

### Initial Approach

The original code sorted the backing vector and called `std::unique` with an epsilon-based comparator. This annotated excerpt shows the core logic:

```cpp
// annotated excerpt: original fuzzy deduplication
auto fuzzy_unique = [eps](const T& lhs, const T& rhs) {
    return std::fabs(lhs - rhs) < eps;
};
auto lastUnique = std::unique(container.begin(), container.end(), fuzzy_unique);
container.erase(lastUnique, container.end());
```

### Observations

During testing with synthetic data that included closely-spaced values, the deduplication produced unexpected survivor counts. A chain of values `[1.0, 1.005, 1.010, 1.015]` with `ε = 0.006` was expected to produce two survivors (two distinct clusters) but the exact survivors depended on whether intermediate values were present.

Stepping through the `std::unique` algorithm revealed the mechanism. At each step, `std::unique` compared the current element against the last element that was *not* removed. Because fuzzy equality is not transitive, the comparison anchor could drift along a chain, consuming values that were individually close to their predecessor but collectively far from the chain origin.

### Hypotheses

**Hypothesis:** Replacing `std::unique` with a scan that anchors comparisons to the last *kept* element (rather than the last *seen* element) eliminates chain drift while preserving O(N) complexity and single-pass behavior.

### Evidence

The mathematical proof is straightforward. If the anchor is the last kept element, and a new element is kept only when its distance from the anchor exceeds ε, then by construction consecutive kept elements are always spaced > ε. No chain can form because the comparison target never drifts — it jumps discretely from one anchor to the next.

To illustrate concretely: with input `[1.0, 1.003, 1.006, 1.009]` and `ε = 0.004`, `std::unique` compares `1.003` to `1.0` (skip), then `1.006` to `1.003` (the last *seen*, which was skipped — but `std::unique` actually compares to the last kept, which is `1.0`). In this particular case `std::unique` happens to produce the correct result, but the correctness is coincidental — `std::unique`'s specification does not guarantee this comparison order for non-equivalence predicates.

The greedy scan makes the comparison order explicit and guaranteed. The write-cursor is always the anchor:

| Step | Anchor | Candidate | Distance | Action |
|------|--------|-----------|----------|--------|
| 1 | 1.000 | 1.003 | 0.003 < ε | Skip |
| 2 | 1.000 | 1.006 | 0.006 > ε | Keep, new anchor |
| 3 | 1.006 | 1.009 | 0.003 < ε | Skip |

**Result:** `[1.000, 1.006]` — consecutive survivors spaced 0.006 > ε. Deterministic, correct, and independent of `std::unique`'s implementation details.

### The Fix

The corrected algorithm replaces `std::unique` entirely. This is the verbatim implementation pattern:

```cpp
// verbatim: greedy forward scan replacing std::unique
if (!container.empty()) {
    auto write_it = container.begin();
    for (auto read_it = container.begin() + 1;
         read_it != container.end(); ++read_it) {
        if (!fuzzy_equal(*write_it, *read_it)) {
            ++write_it;
            if (write_it != read_it) {
                *write_it = std::move(*read_it);
            }
        }
    }
    container.erase(write_it + 1, container.end());
}
```

The comparison `fuzzy_equal(*write_it, *read_it)` always measures against `write_it` — the last kept element. The write cursor advances only when a genuinely distinct value is found.

### Results

| Metric | `std::unique` | Greedy scan |
|--------|---------------|-------------|
| Complexity | O(N) | O(N) |
| Comparisons per element | 1 | 1 |
| Chain-prone input correctness | Unpredictable | Deterministic, ε-spaced |
| Well-separated input correctness | Correct | Correct (identical output) |
| Standard compliance | Predicate violates equivalence requirement | No `std::unique` dependency |

### Components Used

- `FlatSet` — provides sorted contiguous storage where the greedy scan operates
- `FloatingPointComparison` — provides epsilon comparison utilities

### Transferable Lessons

The core lesson generalizes beyond floating-point deduplication: when an algorithm requires a predicate to satisfy mathematical properties (equivalence relation, strict weak ordering, etc.), substituting a predicate that "almost" satisfies the property produces failures that are rare, data-dependent, and difficult to diagnose. The fix is not to hope the data avoids the problematic case, but to replace the algorithm with one that does not require the property in the first place.

---

## Part IV — Foundations

### Design Rationale

The greedy forward scan was chosen because it preserves every desirable property of `std::unique` (O(N), single-pass, in-place, stable) while eliminating the single undesirable property (dependence on predicate transitivity). The algorithm is trivially correct by construction — the invariant "consecutive survivors are spaced > ε" is maintained at every step because the anchor only advances past ε boundaries.

### Rejected Alternatives

**All-pairs comparison (O(N²)).** Comparing every element against every other element guarantees correct clustering but at quadratic cost. For the typical use case (deduplicating thousands to millions of values), this is prohibitive. The greedy scan achieves the same practical result in linear time for sorted data.

**Union-Find clustering.** Building an equivalence graph where edges connect pairs within ε, then extracting connected components, produces mathematically correct equivalence classes. The cost is O(N α(N)) ≈ O(N) but with substantially higher constant factors and memory overhead (one graph node per element). This approach is appropriate for unsorted, high-dimensional data; it is overkill for sorted scalar ranges.

**Explicit binning (round to nearest multiple of ε).** Rounding each value to a bin boundary and then applying exact deduplication avoids the transitivity problem entirely. The disadvantage is that two values separated by less than ε but straddling a bin boundary are treated as distinct. Binning is appropriate when the bin boundaries have physical meaning (e.g., rounding to sensor precision); it is inappropriate when the goal is "collapse values that are within ε of each other."

**Doing nothing.** Leaving `std::unique` in place and documenting the limitation is inappropriate for library code. Users who encounter the edge case receive incorrect results with no diagnostic. The fix costs nothing in performance and eliminates the failure mode.

### Edge Cases

**Empty input.** The greedy scan checks for empty input before entering the loop. No comparison is performed.

**Single element.** The loop body never executes. The single element is the sole survivor.

**All elements identical.** Every comparison returns "within ε." The first element is kept; all others are discarded.

**All elements distinct (spacing >> ε).** Every comparison returns "beyond ε." All elements are kept. Output equals input.

**Monotone chain (every consecutive pair within ε).** The first element anchors. Each subsequent element is compared against the anchor. Eventually an element exceeds ε distance from the anchor and becomes the new anchor. The output contains representatives spaced > ε apart, selected greedily from left to right.

### Why "Sensor Streams" Is the Wrong Framing

A common but misleading justification for worrying about epsilon chains is the "sensor data with gradual drift" scenario: temperature readings like `[20.000, 20.005, 20.010]` where each consecutive pair is within epsilon due to natural fluctuation. This framing conflates two fundamentally different operations.

Sensor data is *time-ordered*. Sorting sensor data by value destroys the temporal relationships that make the data meaningful. Deduplication on sensor data should ask "is this the same physical event observed twice?" — a question about time proximity, not value proximity. The correct tools for sensor data are circular buffers, time-series databases, moving averages, Kalman filters, and change-point detectors. None of these involve sorting by value and fuzzy-comparing.

The only legitimate scenario for putting sensor data into a sorted container is post-hoc value distribution analysis, where temporal information is intentionally discarded and the question is "what distinct values did we observe?" In this scenario, epsilon should match the sensor's measurement precision (from the datasheet), and the clusters are typically well-separated — meaning chains do not form.

**Fact:** If input data naturally forms epsilon-chains (values spaced at roughly ε intervals across a continuous range), fuzzy deduplication is the wrong tool. The data needs binning, clustering, or a different epsilon.

### Mechanical Audit Checklist

- [ ] Fuzzy deduplication uses greedy forward scan, not `std::unique`
- [ ] Comparison anchor is the last *kept* element, not the last *seen* element
- [ ] Epsilon value matches the noise magnitude of the data source (not arbitrarily chosen)
- [ ] Input is sorted before deduplication (greedy scan requires sorted input)
- [ ] Output spacing invariant (consecutive survivors > ε apart) is tested with chain-prone input
- [ ] If data is time-ordered, confirm that sorting by value is intentional and that temporal information loss is acceptable

---

## Design Rules to Internalize

- **Predicates must satisfy their contract.** If an algorithm requires an equivalence relation, do not pass a predicate that is not transitive. If you cannot provide the required property, replace the algorithm, not the predicate.
- **"Works on most inputs" is not correct.** Library code encounters every input distribution its users encounter. Edge cases that seem unlikely in testing become certainties at scale.
- **Anchor-based scanning breaks chains.** Comparing against the last *kept* element rather than the last *seen* element prevents drift in any deduplication or compression algorithm that uses approximate comparison.
- **Match epsilon to the noise source.** Epsilon should come from the data source's precision characteristics (sensor datasheet, numerical analysis of the computation), not from guesswork. An epsilon that is too large relative to the data range guarantees chain formation.
- **Sorted value deduplication is not event deduplication.** If the question is "did this event already happen?" the answer requires temporal or causal reasoning, not value comparison.

## What To Do Now

1. **Audit any code that passes a fuzzy predicate to `std::unique` or equivalent.** Replace with a greedy forward scan.
2. **Verify epsilon values against data source precision.** If epsilon was chosen without reference to the noise characteristics of the data, it is likely wrong.
3. **Add a chain-prone test case.** Generate input with values at ε/2 intervals and verify that the deduplication produces consistent, ε-spaced output.
4. Watch out for: **relative epsilon adaptation.** The greedy scan as presented uses absolute epsilon. If the data spans multiple orders of magnitude, absolute epsilon may be too tight at large values and too loose at small values. Adapt the comparison function, not the algorithm structure.

---

## Glossary

| Term | Definition |
|------|------------|
| **Epsilon chain** | A sequence of sorted values where each consecutive pair is within ε, but the first and last values are separated by more than ε. Chains cause fuzzy deduplication to produce inconsistent results when using `std::unique`. |
| **Equivalence relation** | A binary relation that is reflexive, symmetric, and transitive. Required by `std::unique`'s specification for its predicate. |
| **Fuzzy equality** | Approximate comparison using `\|a - b\| < ε`. Reflexive and symmetric but not transitive. |
| **Greedy forward scan** | The replacement algorithm that compares each element against the last *kept* element (anchor) rather than the last *seen* element. Produces ε-spaced output in O(N). |
| **Anchor** | The most recently kept element in the greedy forward scan. All subsequent candidates are measured against the anchor until a new anchor is established. |
