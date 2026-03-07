# FeatureManager: A Fat-P Library Showcase

## Executive Summary

FeatureManager is a **dependency-resolving feature flag system** with cycle detection, automatic implication propagation, and conflict resolution. Unlike boolean flag maps (no relationships) or simple enable/disable (no dependency tracking), FeatureManager models **Requires**, **Implies**, **Conflicts**, and **MutuallyExclusive** relationships between features, automatically enabling dependencies and detecting invalid configurations. The topological validation ensures no feature can be enabled without its prerequisites.

---

## The Problem Domain

### What Goes Wrong Without It

```cpp
// The manual dependency nightmare
std::map<std::string, bool> features;

void enableRendering() {
    features["rendering"] = true;
    // Must manually enable dependencies!
    features["graphics_context"] = true;
    features["window_system"] = true;
    // Forgot shader_compiler → runtime crash
}

void enableVulkan() {
    features["vulkan"] = true;
    features["rendering"] = true;  // Duplicate logic
    features["graphics_context"] = true;
    // What if vulkan conflicts with opengl?
    // No automatic detection!
}

// Later: someone enables both vulkan and opengl
// System enters inconsistent state
```

| Issue | HPC Impact |
|-------|------------|
| Manual dependency tracking | Easy to forget prerequisites |
| No conflict detection | Invalid configurations possible |
| No implication propagation | Enabling A should auto-enable B |
| No cycle detection | Circular dependencies crash |

### The Standard's Limitation

C++ has no feature flag facility:
- No dependency graph representation
- No topological sorting for dependencies
- No conflict detection
- No standard configuration management

---

## Architecture: Dependency Graph with Validation

### The Mechanism: Relationship-Based Resolution

```cpp
// Four relationship types drive all resolution logic
enum class FeatureRelationship {
    Requires,          // A requires B: enabling A enables B first
    Implies,           // A implies B: enabling A automatically enables B
    Conflicts,         // A conflicts B: A and B cannot both be enabled
    MutuallyExclusive  // Group: only one of {A, B, C} can be enabled
};

// SyncPolicy controls concurrency: SingleThreadedPolicy, MutexSynchronizationPolicy,
// SharedMutexPolicy, or any policy from ConcurrencyPolicies.h
template <typename SyncPolicy = fat_p::SingleThreadedPolicy>
class FeatureManager
{
public:
    // Feature registration
    [[nodiscard]] Expected<void, std::string>
    addFeature(const std::string& name, FeatureCheck check = nullptr);

    // Relationship declaration
    [[nodiscard]] Expected<void, std::string>
    addRelationship(const std::string& from, FeatureRelationship rel, const std::string& to);

    [[nodiscard]] Expected<void, std::string>
    addMutuallyExclusiveGroup(std::initializer_list<std::string> names);

    // State transitions (plan/commit: validates then applies atomically)
    [[nodiscard]] Expected<void, std::string> enable(const std::string& name);
    [[nodiscard]] Expected<void, std::string> disable(const std::string& name);
    [[nodiscard]] Expected<void, std::string> batchEnable(const std::vector<std::string>& names);
    [[nodiscard]] Expected<void, std::string> batchDisable(const std::vector<std::string>& names);

    // Atomic substitution (single plan/commit, one observer notification each)
    [[nodiscard]] Expected<void, std::string> replace(const std::string& from, const std::string& to);
    [[nodiscard]] Expected<void, std::string> forceExclusive(const std::string& feature);

    // Query
    [[nodiscard]] bool isEnabled(const std::string& name) const;

    // Validation
    [[nodiscard]] Expected<void, std::string> validate() const;

    // Observation
    [[nodiscard]] ObserverId addObserver(FeatureObserver callback, int priority = 0);
    [[nodiscard]] ObserverId addBatchObserver(BatchObserver callback, int priority = 0);
    void removeObserver(ObserverId id);

    // Serialization
    [[nodiscard]] Expected<std::string, std::string> toJson() const;
    [[nodiscard]] static Expected<FeatureManager, std::string>
    fromJson(std::string_view jsonStr);
};
```

### Plan/Commit Resolution

`enable()` and `disable()` use a two-phase approach: a *plan* phase computes the full set of features that must change (following Requires, Implies, Conflicts, and MutuallyExclusive relationships transitively and checking for cycles), then a *commit* phase applies all changes atomically under the sync policy's lock. If the plan phase finds a conflict, cycle, or missing dependency, the call returns an error and no state changes.

`replace(from, to)` extends the plan/commit model for `MutuallyExclusive` substitution: it marks `from` as disabled in the plan before running `planEnableRecursive(to)`, so the constraint check sees the correct end-state and the substitution succeeds atomically.

`forceExclusive(feature)` zeros `desiredStates` completely before planning `feature`, so no `MutuallyExclusive` or `Conflicts` check can find a conflicting enabled feature. This is the correct e-stop primitive when the current state is unknown.

---

## Feature Inventory

### 1. Feature Definition

```cpp
FeatureManager fm;

fm.addFeature("rendering");
fm.addFeature("vulkan");
fm.addFeature("opengl");
fm.addFeature("shader_compiler");
fm.addFeature("graphics_context");
```

### 2. Relationship Types

```cpp
// Requires: vulkan needs rendering
fm.addRelationship("vulkan", Requires, "rendering");
fm.addRelationship("vulkan", Requires, "shader_compiler");

// Implies: enabling rendering auto-enables graphics_context
fm.addRelationship("rendering", Implies, "graphics_context");

// Conflicts: vulkan and opengl cannot coexist
fm.addRelationship("vulkan", Conflicts, "opengl");

// MutuallyExclusive: only one renderer at a time
fm.addMutuallyExclusiveGroup({"vulkan", "opengl", "software"});
```

### 3. Validated Enable/Disable

```cpp
auto result = fm.enable("vulkan");
if (!result) {
    log("Enable failed: ", result.error());
    // result.error() is a std::string describing the failure,
    // e.g. "Conflict: vulkan conflicts with opengl (currently enabled)"
    //      "Cycle detected: vulkan → rendering → vulkan"
    //      "Feature 'shader_compiler' not found"
}

// After enable("vulkan"):
// - rendering is enabled (required)
// - shader_compiler is enabled (required)
// - graphics_context is enabled (implied by rendering)
// - opengl cannot be enabled (conflicts)
```

### 4. Cycle Detection

```cpp
fm.addRelationship("A", Requires, "B");
fm.addRelationship("B", Requires, "C");
fm.addRelationship("C", Requires, "A");  // Cycle!

auto result = fm.enable("A");
// result.error() == "Cycle detected: A → B → C → A"
```

### 5. Validation

```cpp
// Check configuration validity; returns Expected<void, std::string>
auto valid = fm.validate();
if (!valid) {
    log("Validation failed: ", valid.error());
}
```

### 6. Observer Pattern

```cpp
ObserverId id = fm.addObserver([](const std::string& feature,
                                            bool enabled,
                                            bool success) {
    log("Feature ", feature, " ", enabled ? "enabled" : "disabled");
});

// Store id and call removeObserver(id) to unregister
fm.removeObserver(id);
```

### 7. Serialization (JSON/GraphViz)

```cpp
// Export to JSON
auto jsonResult = fm.toJson();
if (jsonResult) {
    log(*jsonResult);
}

// Import from JSON (static factory)
auto fmResult = FeatureManager<>::fromJson(config_json);
if (!fmResult) {
    log("Parse error: ", fmResult.error());
}
```

### 8. Thread-Safe Mode

```cpp
// Single-threaded (default, zero overhead)
FeatureManager<SingleThreadedPolicy> fm1;

// Mutex-protected
FeatureManager<fat_p::MutexSynchronizationPolicy> fm2;

// Reader-writer lock (many readers, exclusive writers)
FeatureManager<SharedMutexPolicy> fm3;
```

---

## Why Not Alternatives?

| If You Need... | Why Not std::map<string,bool> | Why Not Manual Graph | Why Not LaunchDarkly | Fat-P Advantage |
|----------------|------------------------------|---------------------|---------------------|-----------------|
| Dependency tracking | ❌ No relationships | ✅ Manual impl | ❌ Boolean only | ✅ Built-in |
| Conflict detection | ❌ No | ✅ Manual impl | ❌ No | ✅ Built-in |
| Cycle detection | ❌ N/A | ❌ Easy to miss | ❌ N/A | ✅ Built-in |
| Zero dependencies | ✅ Standard | ✅ Custom | ❌ Service | ✅ Header-only |
| Type-safe groups | ❌ No | ❌ Manual | ❌ No | ✅ EnumPlus |

**The Sweet Spot:** FeatureManager is the only option combining dependency resolution, conflict detection, cycle detection, and serialization without external services.

---

## The "Forever Stuck" Reality

**Standard Reality:** C++ will never have feature flag management:
- Too application-specific
- No consensus on dependency semantics
- Configuration management considered external

FeatureManager provides dependency-aware feature flags permanently—essential for complex systems with feature interdependencies.

---

## Performance Characteristics

| Operation | Complexity | Notes |
|-----------|------------|-------|
| Add feature | O(1) average | Hash insertion |
| Add relationship | O(log r) | FlatSet insertion |
| Enable (simple) | O(1) average | Hash lookup + update |
| Enable (with deps) | O(d × log n) | d = dependency depth |
| replace(from, to) | O((d_from + d_to) × log n) | d_from = reverse-dep closure depth of from; d_to = enable closure depth of to |
| forceExclusive(f) | O(n + d × log n) | n = total features (zeroing desiredStates); d = dep depth of f |
| Validate | O(n × d × log n) | Full graph traversal |
| Memory per feature | Platform/compiler-dependent | See `results/` for measurements |

### Where Fat-P Wins
- Game engines with feature dependencies
- Plugin systems with compatibility requirements
- Configuration systems needing validation

### Where Fat-P Loses
- Simple boolean flags → `std::map<string, bool>` suffices
- Runtime flag changes from server → LaunchDarkly etc.
- Very large feature sets (1000+) → custom graph DB

---

## Integration Points

```
FeatureManager.h
    ↓ uses
Expected.h              (Error handling)
ConcurrencyPolicies.h   (Thread safety)
JsonLite.h              (Serialization)
EnumPlus.h              (FeatureRelationship, FeatureGroupState)
FlatSet.h               (Relationship storage)
Factory.h               (Feature creation)
ValueGuard.h            (Scoped state changes)
Stringify.h             (Error messages)
```

---

## Final Assessment

FeatureManager delivers on the fat_p promise through three pillars:

### 1. Permanence
C++ won't standardize feature management—too application-specific. FeatureManager provides dependency-aware flags permanently.

### 2. Specialization
Topological dependency resolution, cycle detection with path reporting, and automatic implication propagation handle complex feature graphs. FlatSet storage provides cache-friendly relationship access.

### 3. Control
Four relationship types (Requires, Implies, Conflicts, MutuallyExclusive) model real-world feature dependencies. Thread-safety policies let you choose overhead. Observer pattern enables reactive configuration.

**Architectural Verdict:** FeatureManager transforms feature flags from **isolated booleans** to **dependency-aware, validated configurations**. Enable one feature, get its entire dependency tree—with conflict detection and cycle prevention.

---

*FeatureManager.h (1547 lines) — Fat-P Library*
