# Teaching Pack: StableHashMap / FastHashMap Performance Engineering

This folder contains a set of teaching documents that capture an end-to-end performance engineering process:

- understanding a SwissTable-style hash map design,
- building a benchmark suite that produces *reliable* comparisons,
- using targeted diagnostics to identify a root cause,
- applying a small, mathematically-grounded fix,
- validating via before/after benchmarks,
- and trimming the benchmark suite for fast iteration.

---

## What you already have (the “story”)

### 1) `FastHashMap_Design_History.md`
A design-history style document explaining:
- the SwissTable control-byte + SIMD tag scan approach,
- the H1/H2 split and probing model,
- deletion strategies (tombstone vs backward shift),
- and implementation tradeoffs.

Use this as the **architecture / background reading**.

### 2) `Chasing_Speed.md`
A “performance journal” covering:
- real benchmark results,
- problems encountered,
- and the debugging path that led to fixes.

Use this as the **chronological narrative**.

---

## What this pack adds (the “lesson plan”)

### 3) `CaseStudy_MissDiag_FirstEmptyMask.md`
A focused teaching module on:
- why miss latency is a canary in SwissTables,
- how MissDiag isolates *why* a miss is slow,
- and a specific optimization (masking matches after the first empty) that reduces
  Tag/miss and Eq/miss and therefore miss latency.

Use this as a **classroom/lab handout**.

---

## Suggested reading order for students

1. **FastHashMap_Design_History.md** — skim for concepts, then re-read the SwissTable algorithm section carefully.
2. **CaseStudy_MissDiag_FirstEmptyMask.md** — do the lab/exercises.
3. **Chasing_Speed.md** — read as an “engineering notebook” to see the real-world iteration and decision-making.

---

## Suggested lecture outline (90–120 minutes)

1. SwissTable overview and invariants (15–20 min)
2. Benchmarking methodology: round-robin, randomized order, CPU stability (10–15 min)
3. Why misses matter; what to count (10 min)
4. MissDiag lab: interpret Eq/miss, Tag/miss, Grp/miss (20–30 min)
5. The “first empty” optimization: bitmask math + correctness argument (15–20 min)
6. Re-run + discuss results + regression checklist (15–20 min)

---

## “Slim” benchmark suite philosophy

When iterating quickly, you typically want:
- **Core operations** (Insert, Find hit/miss, Erase, Churn)
- **Pathological erase** (tombstone degradation / churn stability)
- **Slim MissDiag** (just the counters you need to verify miss health)

Everything else can be “extended suite” material.

If you have `benchmark_FatPHashMap_slim.cpp` in your codebase,
that file is designed for this fast loop.

---

## Notes for instructors

- Make students justify any performance claim with *one upstream counter*.
  (“Why did time improve?” → “Because Eq/miss dropped from X to Y.”)
- Require at least 2 sizes (e.g., 100k and 1M) so they see cache + memory effects.
- Encourage “adversarial sets” (e.g., H2-biased misses) to validate robustness.
