---
doc_id: DN-FEATUREMANAGER-001
doc_type: "Design Note"
title: "Atomic Feature Substitution: replace() and forceExclusive()"
fatp_components: ["FeatureManager"]
topics: ["atomic feature substitution", "MutuallyExclusive transition", "e-stop semantics", "plan/commit model", "observer atomicity"]
constraints: ["MutuallyExclusive blocks same-transaction substitution", "batchDisable + batchEnable is not atomic", "Preempts encodes permanent graph structure, not one-time intent"]
cxx_standard: "C++20"
std_equivalent: null
boost_equivalent: null
last_verified: "2026-03-07"
audience: ["C++ developers", "library maintainers", "AI assistants"]
status: "reviewed"
---

# Design Note - Atomic Feature Substitution (replace and forceExclusive)

## Decision

Add two new public methods to `FeatureManager<SyncPolicy>`:

- `replace(from, to)` — atomically disable one feature and enable another within a MutuallyExclusive group.
- `forceExclusive(feature)` — atomically disable every feature not in `feature`'s Requires/Implies closure; e-stop semantics.

Both methods reuse all existing private infrastructure without modifying a single existing line.

---

## Problem

### The MutuallyExclusive substitution problem

The constraint check in `planEnableRecursive` evaluates `Conflicts` and `MutuallyExclusive` against `desiredStates`, which is seeded from live state at the start of every transaction.

If feature `A` is currently enabled and `A` is `MutuallyExclusive` with `B`, enabling `B` fails immediately — because `A` is `true` in `desiredStates` at check time, even when the caller intends to disable `A` as part of the same operation.

The operation `"disable A and enable B atomically, where A and B are mutually exclusive"` cannot be expressed with the existing API.

### Why batchDisable + batchEnable is not a solution

Two sequential calls are not atomic. An observer fires between them. Any code polling `isEnabled()` between the two calls sees a momentarily inconsistent state. Under a mutex policy both calls acquire the same lock in sequence, so another thread can interleave between them.

### Why Preempts was pressed into service (fatp-drone)

`fatp-drone` used `Preempts` edges to approximate "disable `normal_mode` when `safe_mode` is enabled". This fails for three reasons:

1. `Preempts` is a permanent graph edge encoding a structural relationship, not a one-time runtime intent.
2. `MutuallyExclusive + Preempts` on the same pair is rejected at `addRelationship()` time: the contradiction guard fires.
3. `Preempts` cascade-disables the entire reverse-dependency closure of the target, which may be broader than the caller intended.

---

## Decision

### Option A: New plan-seeding overload (rejected)

Add a parameter to `batchEnable()` that pre-marks certain features as disabled before constraint evaluation. Rejected because it adds API surface to a general method, the semantics are subtle, and it conflates two distinct intent patterns (substitution vs. e-stop) into one call.

### Option B: Modify planEnableRecursive to accept a disable set (rejected)

Pass an optional set of features to treat as pre-disabled during constraint evaluation. Rejected for the same reason as Option A, plus it makes the most complex private method harder to reason about.

### Option C: Two focused public methods using the existing plan/commit model (chosen)

`replace` and `forceExclusive` each construct a `TransactionPlan`, manipulate `desiredStates` before calling `planEnableRecursive`, and rely on the existing `validateDesiredState` + `buildTransactionChanges` + observer notification path. No private method changes. No new `FeatureRelationship` enum values.

**Why the plan/commit model handles this correctly:** `planEnableRecursive` evaluates `MutuallyExclusive` against `desiredStates`, not live state. If `desiredStates["A"]` is `false` when the check runs, the substitution succeeds. The existing model was already correct for this — the missing piece was a public entry point that set up `desiredStates` correctly before the check.

---

## Implementation Details

### replace(from, to)

Implementation sequence (all inside the lock):

1. Validate both features exist.
2. Reject `from == to` (explicit error; not a silent no-op).
3. Seed `TransactionPlan` from live state.
4. **Pre-check:** `from` must be currently enabled. Return error if not. Silent no-op would hide caller bugs.
5. `planDisableClosure(from, plan)` — marks `from` and its reverse-dependency closure as `false` in `desiredStates`. No live mutation yet.
6. `planEnableRecursive(to, plan, chain, chainSet)` — succeeds because `from` is `false` in `desiredStates` when the `MutuallyExclusive` check runs.
7. Guard: `to` must be `true` in `plan.desiredStates` (contradictory-batch guard, consistent with `batchEnable`).
8. `validateDesiredState(plan.desiredStates)` — final consistency check.
9. Commit + notify observers outside the lock. `requestedFeature` passed to `BatchObserver` = `to`.

**Error policy for `from` not enabled:** The pre-check at step 4 returns an explicit error. The caller should check `isEnabled(from)` first if conditional behaviour is needed. Relying on `planDisableClosure` to silently no-op when `from` is already disabled would produce a plain `enable(to)` with no indication anything was wrong.

**Error policy for `from == to`:** Returns an explicit error. The operation has no meaningful semantics as a self-replace, and the caller's intent is almost certainly a bug.

### forceExclusive(feature)

Implementation sequence (all inside the lock):

1. Validate `feature` exists.
2. Seed `originalStates` from live state.
3. **Set all entries in `plan.desiredStates` to `false`.** This is the only structural difference from `batchEnable`.
4. **Populate `plan.disableOrder`** from all currently live-enabled features. This step is critical: `planDisableClosure` would early-exit on every feature (all already `false` in `desiredStates`) and never write to `disableOrder`. Without this explicit population, `buildTransactionChanges` would produce an empty change vector, and all disabled features would disappear silently from the observer notification.
5. `planEnableRecursive(feature, plan, chain, chainSet)` — starts from a blank slate, so no `MutuallyExclusive` or `Conflicts` check can find a conflicting enabled feature.
6. `validateDesiredState(plan.desiredStates)`.
7. Commit + notify. `requestedFeature` = `feature`.

**No-op case:** If `feature` is already the only enabled feature, `buildTransactionChanges` produces an empty vector (all `originalStates` == `desiredStates`) and no observers fire.

---

## What Does Not Change

| Component | Change? | Reason |
|-----------|---------|--------|
| `planEnableRecursive` | None | Constraint check evaluates `desiredStates`, not live state — already correct |
| `validateDesiredState` | None | Same reasoning |
| `batchEnable` / `batchDisable` | None | Existing semantics fully preserved |
| `FeatureRelationship` enum | None | `replace`/`forceExclusive` are runtime operations, not graph structure |
| `Preempts` edge | None | Still valid for permanent "if A up, B always killed" relationships |
| `fromJson` / `toJson` | None | New methods are not serialized |

---

## Observer Semantics

Both methods use `buildTransactionChanges(plan)` identically to `batchEnable`. The `requestedFeature` argument passed to `BatchObserver` is `to` for `replace` and `feature` for `forceExclusive`.

The `changes` vector contains disable records first (from `disableOrder`), then enable records (from `enableOrder`). This matches the existing mixed-direction pattern established by `Preempts` transactions. Existing observers require no changes.

---

## Caller Migration Notes

### fatp-balancer FeatureSupervisor

The current `applyChanges()` pattern:

```cpp
batchDisable({currentAlert, currentPolicy});
enable(newAlert);
```

Can be replaced with:

```cpp
replace(currentAlert, newAlert);
// policy feature disable remains a separate step:
// it is not part of a MutuallyExclusive group with the alert
```

Alternatively, `forceExclusive` can be used if the intent is "drop everything and activate this alert unconditionally".

### fatp-drone e-stop handler

Replace the `Preempts` graph edge approach with:

```cpp
// On e-stop trigger:
manager.forceExclusive("safe_mode");

// On recovery:
manager.replace("safe_mode", "normal_mode");
```

The `Preempts` relationship from `safe_mode` to `normal_mode` can be removed from the graph. The latched-inhibit property (blocking re-enable while the preemptor is up) is no longer needed because `forceExclusive` + `replace` make the intent explicit at each call site.

---

## Alternatives Considered but Not Implemented

### "Allow MutuallyExclusive + Preempts on the same pair"

The contradiction guard at `addRelationship()` (line 647) rejects this combination because `Preempts` encodes "enabling A force-kills B's entire reverse-dependency closure permanently", while `MutuallyExclusive` encodes "A and B cannot coexist." Combining them on the same directed edge produces ambiguous semantics about cascade scope and inhibit behaviour. The guard should stay.

### "Implicit disable of conflicting features during enable"

Making `planEnableRecursive` automatically disable any `MutuallyExclusive` peer that is currently enabled. Rejected: it converts a constraint violation into a silent state change, breaking the principle that no requested operation silently alters features the caller did not name. Callers should be explicit about what they are turning off.

---

*Fat-P Library — FeatureManager — Phase 10+*
