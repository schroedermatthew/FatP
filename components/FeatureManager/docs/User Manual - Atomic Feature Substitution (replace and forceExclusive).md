---
doc_id: UM-FEATUREMANAGER-002
doc_type: "User Manual"
title: "Atomic Feature Substitution: replace() and forceExclusive()"
fatp_components: ["FeatureManager"]
topics: ["atomic feature substitution", "MutuallyExclusive transition", "e-stop pattern", "replace API", "forceExclusive API"]
constraints: ["MutuallyExclusive prevents naive sequential disable/enable", "observer atomicity", "plan/commit model"]
cxx_standard: "C++20"
std_equivalent: null
boost_equivalent: null
last_verified: "2026-03-07"
audience: ["C++ developers", "AI assistants"]
status: "reviewed"
---

# User Manual - Atomic Feature Substitution (replace and forceExclusive)

> **Placement:** This section belongs in the *User Manual — FeatureManager* API Reference,
> after the `batchDisable()` entry and before the `isEnabled()` entry.

---

## The Problem These Methods Solve

`MutuallyExclusive` means exactly what it says: features in the same group cannot be enabled at the same time. That constraint is enforced during the planning phase, against `desiredStates`, before any live mutation occurs.

If `A` is currently enabled and `A` is `MutuallyExclusive` with `B`, calling `enable("B")` fails — because `A` is still `true` in `desiredStates` when the conflict check runs.

```cpp
FeatureManager<> fm;
(void)fm.addFeature("ModeA");
(void)fm.addFeature("ModeB");
(void)fm.addRelationship("ModeA", FeatureRelationship::MutuallyExclusive, "ModeB");

(void)fm.enable("ModeA");

// This fails: "ModeB is mutually exclusive with ModeA"
auto res = fm.enable("ModeB");
// res.has_value() == false
```

Two sequential calls don't fix it:

```cpp
fm.disable("ModeA");  // An observer fires here.
fm.enable("ModeB");   // Another observer fires here.
                      // Any code polling isEnabled() between these two calls
                      // sees neither mode active — an inconsistent state.
```

`replace()` and `forceExclusive()` solve this by manipulating `desiredStates` before the constraint check runs, so the entire transition is planned and committed in a single atomic operation.

---

## `replace(from, to)`

### Signature

```cpp
[[nodiscard]] Expected<void, std::string>
replace(const std::string& from, const std::string& to);
```

### What It Does

Atomically swaps one enabled feature for another.

1. Marks `from` (and its reverse-dependency closure) as disabled in the plan.
2. Plans `to` (and its Requires/Implies closure) — the `MutuallyExclusive` check now sees `from` as disabled and succeeds.
3. Validates the full desired state.
4. Commits and notifies observers exactly once.

The `BatchObserver` receives `requestedFeature = to` and a `changes` vector containing both the disable records for `from`'s closure and the enable records for `to`'s closure.

### Error Conditions

| Condition | Error |
|-----------|-------|
| `from` not found | Feature not found error |
| `to` not found | Feature not found error |
| `from` is not currently enabled | Explicit error — check `isEnabled(from)` first if conditional behaviour is needed |
| `from == to` | Explicit error — self-replace is not meaningful |
| `to`'s Requires closure has an unmet Conflicts constraint | Planning error — no state change |

### When to Use

Use `replace()` when the caller knows which feature is currently active and wants to substitute a specific alternative. It is the right tool for controlled mode transitions within a `MutuallyExclusive` group.

### When Not to Use

Do not use `replace()` as an e-stop. If an emergency requires clearing an unknown set of active features and activating a specific safe state, `forceExclusive()` is correct — it does not require the caller to know what is currently active.

### Example: Mode Switch

```cpp
FeatureManager<> fm;
(void)fm.addFeature("normal_mode");
(void)fm.addFeature("safe_mode");
(void)fm.addRelationship("normal_mode", FeatureRelationship::MutuallyExclusive, "safe_mode");

(void)fm.enable("normal_mode");

// Atomic transition: normal_mode goes off, safe_mode comes on.
// One observer notification, no intermediate inconsistent state.
auto res = fm.replace("normal_mode", "safe_mode");
if (!res)
{
    log("Transition failed: ", res.error());
}
```

### Example: With Requires Chains

`replace()` correctly handles the reverse-dependency closure of `from`. Features that Require or Imply `from` are transitively disabled (they can no longer hold up their own dependency chain). Features that `to` Requires are brought up automatically.

```cpp
// Graph:
//   motor_mix  --Requires-->  esc
//   normal_mode --Requires-->  motor_mix   (so enabling normal_mode brings up esc and motor_mix)
//   normal_mode  <MutuallyExclusive>  safe_mode
//   safe_mode  --Requires-->  network_stub

(void)fm.enable("normal_mode");
// enabled: normal_mode, motor_mix, esc

auto res = fm.replace("normal_mode", "safe_mode");
// After replace():
//   enabled:  safe_mode, network_stub
//   disabled: normal_mode, motor_mix, esc
//             (motor_mix and esc are in normal_mode's reverse-dep closure)
```

### Example: Observer Receives Both Changes

```cpp
(void)fm.addBatchObserver([](const std::string& requested,
                             const std::vector<FeatureChange>& changes,
                             bool) {
    // requested == "safe_mode" (the 'to' argument)
    // changes contains:
    //   {name: "normal_mode", oldState: true,  newState: false}
    //   {name: "motor_mix",   oldState: true,  newState: false}
    //   {name: "esc",         oldState: true,  newState: false}
    //   {name: "network_stub",oldState: false, newState: true}
    //   {name: "safe_mode",   oldState: false, newState: true}
});

(void)fm.replace("normal_mode", "safe_mode");
```

---

## `forceExclusive(feature)`

### Signature

```cpp
[[nodiscard]] Expected<void, std::string>
forceExclusive(const std::string& feature);
```

### What It Does

Disables every feature not in `feature`'s Requires/Implies closure, atomically.

1. Snapshots live state into `originalStates`.
2. Sets **all** entries in `desiredStates` to `false` and records all currently-enabled features in `disableOrder`.
3. Plans `feature` from a blank slate — no `MutuallyExclusive` or `Conflicts` check can find a conflicting enabled feature because every entry in `desiredStates` is `false`.
4. Validates, commits, notifies. `requestedFeature` for `BatchObserver` = `feature`.

If `feature` is already the only enabled feature, `buildTransactionChanges` produces an empty change vector and no observers fire.

### Error Conditions

| Condition | Error |
|-----------|-------|
| `feature` not found | Feature not found error |
| `feature`'s own Requires closure has an internal `Conflicts` edge | Planning error — broken graph, correctly rejected |

### When to Use

Use `forceExclusive()` for unconditional activation of a known-safe state when the current system state is unknown or too complex to enumerate. It is the correct e-stop primitive: the caller does not need to know what is currently active.

### When Not to Use

Do not use `forceExclusive()` when the caller knows the current state and the transition is controlled. `replace()` is cleaner and more auditable in that scenario. `forceExclusive()` is a sledgehammer — appropriate for emergencies, not routine mode switches.

### Example: E-Stop Activation

```cpp
// Graph has many active features: flight modes, sensors, navigation stack.
// On e-stop trigger, disable everything and activate safe_mode unconditionally.

auto res = fm.forceExclusive("safe_mode");
// After forceExclusive:
//   safe_mode: enabled
//   network_stub: enabled (Requires chain of safe_mode)
//   everything else: disabled, regardless of what was previously active
```

### Example: Recovery with replace

After a `forceExclusive`, recovery transitions use `replace`:

```cpp
// E-stop
fm.forceExclusive("safe_mode");

// ... operator confirms system is ready for recovery ...

// Recovery: safe_mode off, normal_mode on — atomic
fm.replace("safe_mode", "normal_mode");
```

### Example: No-Op Case

```cpp
// Only safe_mode is active; nothing else is on.
auto res = fm.forceExclusive("safe_mode");
// res.has_value() == true
// No observers fired — state was already exclusive.
```

### Example: Observer Receives All Disabled Features

```cpp
(void)fm.addBatchObserver([](const std::string& requested,
                             const std::vector<FeatureChange>& changes,
                             bool) {
    // requested == "safe_mode"
    // changes contains a disable record for every feature that was active,
    // plus enable records for safe_mode and its Requires chain.
    for (const auto& c : changes)
    {
        log(c.name, ": ", c.oldState, " -> ", c.newState);
    }
});

fm.forceExclusive("safe_mode");
```

---

## Choosing Between replace() and forceExclusive()

| Question | Answer |
|----------|--------|
| Do I know which specific feature is currently active? | Use `replace()` |
| Is this a controlled transition between known modes? | Use `replace()` |
| Is this an emergency where I don't know (or care) what's active? | Use `forceExclusive()` |
| Do I want to clear an unbounded set of active features? | Use `forceExclusive()` |
| Are the two features `MutuallyExclusive`? | Either works; `replace()` is cleaner |
| Are the two features **not** `MutuallyExclusive`? | `replace()` still works — just disables A then enables B |

---

## Thread Safety

Both methods acquire the internal lock for the entire plan/commit phase and release it before notifying observers. Observer notifications are identical in structure to `batchEnable` and `batchDisable`. Observers may safely call back into the manager (reentrant use applies the same rules as for all observer callbacks).

---

## Interaction with Preempts

`replace()` and `forceExclusive()` do not interact with `Preempts` edges directly — they use `planDisableClosure` and `planEnableRecursive`, which handle `Preempts` exactly as `batchEnable` does. If `feature` in `forceExclusive` has `Preempts` edges, the targets are disabled as part of `planEnableRecursive`'s Preempts cascade, which is redundant (they were already zeroed) but harmless.

If the caller has been using `Preempts` to approximate e-stop behaviour (as `fatp-drone` did), those edges can be removed after migrating to `forceExclusive` + `replace`. See `Design Note - Atomic Feature Substitution` for the migration pattern.

---

## Relationship to batchEnable / batchDisable

`replace` and `forceExclusive` are not wrappers around `batchEnable`/`batchDisable`. They are independent transactional methods that share the same private infrastructure. They follow identical plan/commit/notify semantics and produce observer notifications with the same structure.

| Method | desiredStates seeded from | Pre-plan manipulation |
|--------|--------------------------|----------------------|
| `batchEnable` | live state | none |
| `batchDisable` | live state | marks requested features `false` |
| `replace` | live state | `planDisableClosure(from)` before `planEnableRecursive(to)` |
| `forceExclusive` | live state (`originalStates` only) | zero all `desiredStates`, populate `disableOrder` |

---

*Fat-P Library — FeatureManager — Phase 10+*
