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
enum class FeatureRelationship {
    Requires,          // A requires B → enabling A requires B enabled first
    Implies,           // A implies B → enabling A automatically enables B
    Conflicts,         // A conflicts B → A and B cannot both be enabled
    MutuallyExclusive  // Group: only one of {A, B, C} can be enabled
};

template<typename ConcurrencyPolicy = SingleThreadedPolicy>
class FeatureManager {
    struct Feature {
        std::string name;
        bool enabled = false;
        FlatSet<Relationship> relationships;
    };
    
    std::map<std::string, Feature> features_;
    
public:
    void addFeature(const std::string& name);
    void addRelationship(const std::string& from, 
                         FeatureRelationship rel,
                         const std::string& to);
    
    Expected<void, FeatureError> enable(const std::string& name);
    Expected<void, FeatureError> disable(const std::string& name);
    
    bool validate() const;  // Check for conflicts/missing deps
};
```

### Dependency Resolution Algorithm

```cpp
Expected<void, FeatureError> enable(const std::string& name) {
    // 1. Check if feature exists
    auto it = features_.find(name);
    if (it == features_.end()) {
        return {unexpect, FeatureError::NotFound};
    }
    
    // 2. Check for conflicts
    for (const auto& rel : it->second.mRelationships) {
        if (rel.type == Conflicts && features_[rel.target].mEnabled) {
            return {unexpect, FeatureError::Conflict};
        }
    }
    
    // 3. Enable required dependencies first (recursive)
    for (const auto& rel : it->second.mRelationships) {
        if (rel.type == Requires) {
            auto result = enable(rel.target);  // Recursive
            if (!result) return result;
        }
    }
    
    // 4. Enable this feature
    it->second.mEnabled = true;
    
    // 5. Enable implied features
    for (const auto& rel : it->second.mRelationships) {
        if (rel.type == Implies) {
            enable(rel.target);  // Best-effort
        }
    }
    
    return {};
}
```

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
fm.addMutuallyExclusive({"vulkan", "opengl", "software"});
```

### 3. Safe Enable/Disable

```cpp
auto result = fm.enable("vulkan");
if (!result) {
    switch (result.error()) {
        case FeatureError::NotFound:
            log("Feature not registered");
            break;
        case FeatureError::Conflict:
            log("Conflicts with enabled feature");
            break;
        case FeatureError::CyclicDependency:
            log("Circular dependency detected");
            break;
        case FeatureError::MissingDependency:
            log("Required feature cannot be enabled");
            break;
    }
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
// result.error() == FeatureError::CyclicDependency
// Error message: "Cycle detected: A → B → C → A"
```

### 5. Validation

```cpp
// Check configuration validity
bool valid = fm.validate();

// Get detailed validation report
auto report = fm.getValidationReport();
for (const auto& issue : report.issues) {
    log(issue.severity, ": ", issue.message);
}
```

### 6. Observer Pattern

```cpp
fm.addObserver([](const std::string& feature, bool enabled) {
    log("Feature ", feature, " ", enabled ? "enabled" : "disabled");
});

// Scoped observation with RAII
{
    auto subscription = fm.observe([](auto...) { /* ... */ });
    // Observer removed when subscription destroyed
}
```

### 7. Serialization (JSON/GraphViz)

```cpp
// Export to JSON
std::string json = fm.toJson();
// {"features":[{"name":"vulkan","enabled":true,...}],"relationships":[...]}

// Import from JSON
fm.loadFromJson(config_json);

// Export to GraphViz DOT for visualization
std::string dot = fm.toDot();
// digraph { vulkan -> rendering [label="requires"]; ... }
```

### 8. Thread-Safe Mode

```cpp
// Single-threaded (default, zero overhead)
FeatureManager<SingleThreadedPolicy> fm1;

// Mutex-protected
FeatureManager<MutexPolicy> fm2;

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
| Add feature | O(log n) | Map insertion |
| Add relationship | O(log r) | FlatSet insertion |
| Enable (simple) | O(log n) | Map lookup + update |
| Enable (with deps) | O(d × log n) | d = dependency depth |
| Validate | O(n × d × log n) | Full graph traversal |
| Memory per feature | ~550 bytes | With 5 relationships |

### Where Fat-P Wins
- Game engines with feature dependencies
- Plugin systems with compatibility requirements
- Configuration systems needing validation

### Where Fat-P Loses (Honesty Builds Trust)
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
