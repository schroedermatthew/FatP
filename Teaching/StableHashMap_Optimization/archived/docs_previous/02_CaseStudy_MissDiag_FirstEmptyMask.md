# Case Study: Diagnosing SwissTable Miss Latency with MissDiag (and the “First Empty” Optimization)

This is a teaching-oriented writeup of a real optimization cycle for a SwissTable-style hash map
(**StableHashMap**, a reference-stable map with node storage and SIMD tag scanning).

It focuses on one very specific (and very common) performance pathology:

> **On a miss, doing extra work for tag matches that occur *after the first empty slot in a SIMD group*.**

That work is provably unnecessary, and it shows up clearly once you instrument the miss path.

---

## 1) Why miss performance is the canary

In SwissTable-style maps, **misses are structurally simpler than hits**:

- You compute a hash.
- You probe groups (SIMD control-byte windows).
- You may do a few cheap tag compares.
- You stop as soon as you see an *empty* control byte in the probe sequence.

So on a “good” implementation, a miss is usually:
- **1 group** (or a small handful) plus
- **0–very few** equality comparisons, because tag matches are rare.

If miss latency gets large, you can usually attribute it to one of two buckets:

1. **Too many groups scanned** (probe sequence too long / table too full / clustering).
2. **Too many “candidate checks” inside groups** (too many tag matches → extra node derefs + `key_equal` calls).

That second bucket is exactly what MissDiag is designed to isolate.

---

## 2) The diagnostic tool: MissDiag

### What MissDiag measures (per miss)

MissDiag breaks the miss path into counters and timing:

- **Median(ns)**: median ns per miss.
- **Eq/miss**: number of `key_equal` calls per miss (candidate comparisons).
- **Hash/miss**: number of hash computations per miss (should be ~1).
- **Grp/miss**: how many SIMD groups you scanned per miss.
- **Tag/miss**: how many tag matches you processed per miss (proxy for extra candidate checks).

A *healthy* miss path for a node-based SwissTable often looks like:

- Grp/miss ≈ 1.0 (at moderate load)
- Tag/miss ≈ 0.0–0.02
- Eq/miss ≈ Tag/miss (because you usually only call `key_equal` after a tag match)

### Why this matters specifically for StableHashMap

StableHashMap is **reference-stable**: it stores elements in nodes, and the table stores references/indices.
That means a false-positive tag match can trigger:

- a node dereference (pointer chase)
- a `key_equal` call (potentially expensive for strings / custom keys)

So MissDiag is not just “nice to have” — it is the fastest way to answer:

> “Are we paying for unnecessary node dereferences on misses?”

---

## 3) The core invariant: the first empty terminates the search

SwissTable-style probing has a key invariant:

> **During lookup, the probe sequence terminates at the first empty slot.**

Within a SIMD group, you typically compute:
- a **match mask** for tags (H2 fingerprints)
- an **empty mask** for empty control bytes

### The subtlety

A common (and reasonable-looking) implementation structure is:

1. Compute `match_mask = match(h2)` across the group.
2. Iterate candidates indicated by `match_mask` (node deref + `key_equal`).
3. Compute `empty_mask = match_empty()` and if any empty exists → terminate.

The issue:
- If there is an empty at position *p* in the group,
  then **any occupied slots after p are irrelevant for this probe window**.
- On a miss, it is guaranteed you will terminate at that empty, so processing matches after it is wasted work.

This is *exactly* the “Option B” idea:

> **Stop processing tag matches after the first empty in the group.**

---

## 4) The bit-mask math

Assume your SIMD group produces two bitmasks:

- `uint32_t match_mask` — bit i is 1 if slot i's tag matches H2
- `uint32_t empty_mask` — bit i is 1 if slot i is EMPTY

Let `first_empty_bit = ctz(empty_mask)` (count trailing zeros).
Then the valid range inside the group is indices `[0, first_empty_bit)`.

So you can mask away everything after the first empty:

```cpp
match_mask &= ((1u << first_empty_bit) - 1);
```

### Guarding edge cases

- If `empty_mask == 0`, there is no empty in the group → do not mask.
- `ctz(x)` is only valid if `x != 0`.

### A branch-friendly alternative (no `ctz`)

A neat trick: isolate the lowest set bit with `empty_mask & -empty_mask`.

```cpp
uint32_t before_first_empty =
    empty_mask ? ((empty_mask & -empty_mask) - 1u) : 0xFFFFFFFFu;

match_mask &= before_first_empty;
```

This avoids shifting by a variable amount and avoids `ctz`.

---

## 5) Implementation sketch (find_slot)

Pseudocode for the “Option B” miss path:

```cpp
match_mask = group.match(h2);
empty_mask = group.match_empty();

if (empty_mask) {
    match_mask &= ((empty_mask & -empty_mask) - 1u);
}

for each bit in match_mask:
    // node deref + key_equal

if (empty_mask) return NOT_FOUND;
advance to next group in probe sequence;
```

You still need the empty check at the end (or beginning) to terminate,
but you **must not** chase candidates that are past the earliest empty in that group.

---

## 6) How to validate with benchmarks (what to look for)

After applying the mask:

### Expected counter movement

- **Tag/miss should drop** (often by a lot).
- **Eq/miss should drop proportionally** (since equality checks come from tag matches).
- **Median(ns) should drop** — especially at larger N, where extra derefs are more expensive.

### A concrete before/after snapshot (example)

From one run at **N=1,000,000**, reserve=1,000,000:

- Before:
  - StableHashMap+SM64: ~10.5 ns/miss, Eq/miss ~0.12, Tag/miss ~0.12
- After “first empty” masking:
  - StableHashMap+SM64: ~3.3 ns/miss, Eq/miss ~0.01, Tag/miss ~0.01

The key is not the absolute ns (machines vary), it’s the **shape**:
- Eq/miss and Tag/miss collapse,
- and miss latency follows.

---

## 7) Teaching plan (how to turn this into a lab)

### Exercise A — Identify the symptom
1. Run Core + MissDiag at N=10k / 100k / 1M.
2. Record:
   - Find(miss) ns/op (core)
   - Eq/miss and Tag/miss (MissDiag)

### Exercise B — Form the hypothesis
- If Grp/miss is ~1 but Eq/miss is elevated → “we’re doing unnecessary candidate work”.

### Exercise C — Apply the invariant
Implement the “first empty” masking and re-run.

### Exercise D — Confirm the mechanism
Show that:
- Eq/miss falls first,
- and ns/miss falls as a consequence.

This teaches the most important lesson in performance engineering:

> **Measure an event that is causally upstream of the time you care about.**

---

## 8) Checklist for future regressions

When misses regress, check in this order:

1. **Hash quality / H2 derivation** (tag collisions drive candidate work).
2. **First-empty termination correctness** (don’t process candidates past the stop point).
3. **Load factor / reserve policy** (Grp/miss rises when you’re too full).
4. **Control-byte invariants** (sentinel / mirroring bugs can explode probe lengths).

---

## Where this fits in the larger teaching set

- For the big-picture narrative, see **Chasing_Speed.md** (performance journal / bug hunts).
- For the SwissTable fundamentals and control-byte design, see **FastHashMap_Design_History.md**.
