# The Case of the Slow Miss  
*A StableHashMap performance detective story — with math, instrumentation, and benchmarks.*

## Prologue: the one operation that should be boring

A hash map **miss** is supposed to be the easy path.

- No value to return.
- No insertion to perform.
- No rehashing.
- Ideally: “check a small amount of metadata, see an empty slot, stop.”

So when our **reference‑stable** hash map (“StableHashMap”: pointers/references stay valid across insert/reserve) started losing badly on **Find(miss)** at large sizes, it wasn’t just disappointing — it was *suspicious*.

Because if a miss is slow, it usually means you’re doing work you didn’t intend to do.

This is the story of how we hunted that work down, using:

- a benchmark harness designed to resist “benchmark lies,”  
- a purpose‑built **Miss Diagnostics** counter,
- a little probability,
- and, in the end, a one‑line bitmask that changed everything.

---

## Act I: the smoke test that started it all

The benchmark suite is intentionally strict:

- **Round‑robin execution**: each measured run executes exactly one timed iteration per library, and the library order is randomized per run.
- **CPU frequency stability**: the harness waits until frequency variance is below a threshold before timing (critical on Windows with aggressive power management).
- **Medians** are the primary statistic.

This is the “don’t trust your stopwatch” chapter from *Chasing Speed* turned into code.

In that harness, at **N = 1,000,000**, the original build produced a miss result that looked like this:

| Map | Find(miss) ns/op (Core) |
|---|---:|
| **StableHashMap + SplitMix64** | **17.61** |
| **StableHashMap[Block] + SM64** | **13.16** |
| **boost::unordered_node_map + SM64** | **5.52** |

A reference‑stable map is allowed to be slower than a flat map.  
But *on misses*, why would it be **~3× slower** than boost’s node map?

That question became the investigation.

---

## Act II: first suspects — hash quality and tag correlation

Before we assumed “the table logic is wrong,” we started with the two most common performance killers in open addressing:

### 1) Bad hashes

On many platforms, `std::hash<int>` behaves like identity for integers.  
That can create patterns (especially in the low bits) that are invisible at small N and catastrophic at large N.

So FastHashMap and StableHashMap gained a built‑in **SplitMix64 finalizer** by default, with an opt‑out marker (`is_avalanching`) for already‑good hashes.

That’s not a micro‑optimization — it’s defensive engineering.

### 2) Using the wrong bits for H2

StableHashMap uses a small **H2 tag** (7 bits) stored in the control byte array.  
SIMD compares H2 across the group to quickly find “maybe” candidates.

But the table index is also derived from the hash:

- bucket index uses **low bits** via `hash & mask`

If H2 also uses low bits, you risk **correlation** between:

- “where the key lands” and
- “which tags look similar”

That can inflate tag matches → inflate node dereferences → inflate misses.

So Option A moved H2 extraction to the **high bits** of the hash, decoupling H2 from the bucket index.

### The “don’t guess” check: H2‑biased miss sets

To test H2 sensitivity without special keys or contrived inputs, MissDiag generates two additional miss sets by biasing the SplitMix64 bits:

- **H2‑biased (low7)**
- **H2‑biased (high7)**

If one of those suddenly increases **Tag/miss** and **Eq/miss**, you’ve found a false‑positive sensitivity.

These stress tests helped keep us honest: any “fix” had to work on random misses *and* on biased misses.

---

## Act III: build a microscope, not a bigger hammer

Even after the “hash/H2 hygiene” improvements, we still saw misses that didn’t *feel* like they were terminating early.

At this point there were too many plausible “suspects”:

- probe sequence behavior (cluster length),
- SIMD tag filtering (false positives),
- node indirections (pointer chasing),
- or a subtle “extra work on the miss path” bug.

Guessing is expensive. So instead, we built a microscope.

### MissDiag: turning a miss into a structured measurement

The **Miss Diagnostics** mode isolates misses and reports:

- **ns/miss**: latency per miss.
- **Eq/miss**: how many `key_equal` calls we trigger per miss (candidate compares).
- **Hash/miss**: hash computations per miss (should be ~1).
- **Grp/miss**: probe groups visited per miss.
- **FullSlots/miss**: occupied slots scanned across visited groups.
- **FullGrp/miss**: groups with *no empties* encountered (hard clusters).
- **Tag/miss**: tag matches per miss (how often the H2 filter says “maybe”).

This instrument did two jobs at once:

1. It told us *where the time went* (candidate compares vs probing vs hashing).
2. It told us whether the “miss should terminate early” expectation held.

---

## Act IV: the numbers that didn’t make sense (until they did)

Here is the critical before/after comparison at **N = 1,000,000, reserve = 1,000,000**, random misses:

| Scenario | Median(ns) | Eq/miss | Tag/miss | Grp/miss | FullSlots/m |
|---|---:|---:|---:|---:|---:|
| **Before Option B** | **10.54** | **0.12** | **0.12** | 1.00 | 15.27 |
| **After Option B** | **3.33** | **0.01** | **0.01** | 1.00 | 15.27 |

Nothing about the table occupancy changed (FullSlots/m stayed **15.27**).  
But **candidate work collapsed by ~12×** and latency by **~3×**.

That “Eq/miss collapse” was the tell.

### A quick piece of math to interpret those counters

StableHashMap’s tag is 7 bits, so with a well‑mixed hash:

- **P(tag match) ≈ 1/128** per occupied slot

So expected tag matches scale with “how many occupied slots you actually consider.”

Now look at the *before* result:

- FullSlots/miss ≈ 15.27  
- 15.27 / 128 ≈ 0.119

…and Tag/miss was **0.12**.

That is **too perfect**.

It suggests we were effectively processing tag matches across *all occupied slots in the group window* — even on misses that should terminate early when an empty appears.

But why would a miss “consider” those slots at all?

---

## Act V: the real culprit — ordering

Here’s the key structure in the original `find_slot()` loop (simplified):

```cpp
match_mask = match(h2);      // candidates across the whole group

while (match_mask) {         // compare each candidate
  deref node;
  key_equal(...);
  clear lowest bit;
}

if (match_empty()) return npos;  // only now do we learn the search should stop
```

Notice the order:

1. Compute tag matches across all 32 positions.
2. Process every match (and potentially dereference nodes).
3. Only then check whether there was an empty.

But on a **miss**, once there’s an empty in the group window, the search terminates — and any occupied slots *after the first empty* cannot contain the key for this probe window.

So tag matches after the first empty are guaranteed false positives.  
And each false positive is a node dereference + key compare — the “slow miss.”

---

## Act VI: Option B — the one‑mask fix

Option B implements a simple rule:

> **Stop processing tag matches after the first empty in the group.**

Mechanically:

1. Compute `match_mask` for H2.
2. Compute `empty_mask`.
3. Find the **first empty** bit in the group.
4. Mask off any tag matches *after* that bit.

Pseudocode:

```cpp
empty_mask = match_empty();
if (empty_mask) {
  first_empty_bit = lowest_set_bit(empty_mask);
  match_mask &= (first_empty_bit - 1);  // keep only bits before the first empty
}
```

Now the algorithm only performs candidate comparisons in the prefix that is actually reachable for this probe.

---

## Act VII: validation — the “this can’t be a fluke” checklist

A good optimization isn’t “faster once.” It’s:

- faster across sizes,
- stable across CPU states,
- and consistent with counters.

After Option B:

- **Core Find(miss)** at **N = 1,000,000** dropped dramatically:
  - StableHashMap + SplitMix64: **17.61 → 3.33 ns**
  - StableHashMap[Block] + SM64: **13.16 → 3.06 ns**
- MissDiag showed **Eq/miss** dropping from **0.12 → 0.01** at N=1,000,000.

And the math now lines up with the *expected* work:

At N=1,000,000 reserve=1,000,000, capacity rounds up to a power of two (≈ 2,097,152), so the actual load factor is about:

- LF ≈ 1,000,000 / 2,097,152 ≈ 0.477  
- P(empty) ≈ 0.523

If empties are common, the expected number of full slots before the first empty is roughly:

- E[full-before-empty] ≈ LF / (1−LF) ≈ 0.477 / 0.523 ≈ 0.91

Expected tag matches before the first empty:

- 0.91 / 128 ≈ 0.007

…which is exactly the order of magnitude we saw (**Tag/miss ≈ 0.01**).

That’s the “numbers snapped into focus” moment.

---

## Act VIII: the supporting cast — harness design and the slim bench

Once MissDiag existed, we didn’t need to run the entire suite for every hypothesis.

So we slimmed the benchmark down to the three loops that mattered for this investigation:

- **Core operations**
- **Pathological erase churn**
- **Slim MissDiag**

That makes “hypothesis → patch → verify” cycles much faster, without losing rigor.

---

## Epilogue: what this teaches (beyond hash maps)

This investigation is really a template for performance engineering:

1. **Start with a symptom** that is hard to dismiss (“miss is slow”).
2. **Build instrumentation** that turns hunches into measurements (MissDiag counters).
3. **Use math as a sanity check**, not as a substitute for data.
4. **Change one thing at a time**, and demand that counters move in the expected direction.
5. **Bake the lesson into documents**, so the next person doesn’t have to rediscover it.

If *Chasing Speed* is the mindset, and *Design History* is the map of past decisions,  
then this case study is the moment where measurement, math, and code all agreed on the same truth:

> The empties were there all along.  
> We just weren’t letting them end the search soon enough.

---

## Where this fits in the Teaching Pack

This narrative is intended as the “front door” document.

For deeper detail, cross‑reference:

- **Chasing_Speed.md** — the broader philosophy of measurement‑driven iteration
- **FastHashMap_Design_History.md** — the design evolution and tradeoffs
- **CaseStudy_MissDiag_FirstEmptyMask.md** — the technical deep dive and implementation notes
