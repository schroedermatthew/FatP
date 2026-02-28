---
doc_id: UM-FEATUREMANAGER-001
doc_type: "User Manual"
title: "FeatureManager"
fatp_components: ["FeatureManager"]
topics: ["feature flags", "feature dependencies", "feature relationships", "feature validation", "callback factory", "observer pattern", "automatic resolution", "dependency graph", "JSON configuration", "thread-safe features"]
constraints: ["dependency cycle detection", "feature state consistency under concurrent access", "observer notification ordering", "relationship validation cost"]
cxx_standard: "C++20"
std_equivalent: null
boost_equivalent: null
build_modes: ["Debug", "Release"]
last_verified: "2026-02-15"
audience: ["C++ developers", "AI assistants"]
status: "reviewed"
---

# User Manual - FeatureManager

**Scope:** Complete usage guide for `fat_p::FeatureManager`: feature registration, dependency and relationship management (requires, conflicts, implies), automatic resolution, callback factory system, observer notifications, JSON configuration, thread safety, and diagnostic APIs.

**Not covered:**
- A/B testing frameworks
- Feature flag distribution systems (LaunchDarkly, etc.)
- Remote configuration management
- Gradual rollout strategies

**Prerequisites:** C++20; understanding of feature flags and their use in software development; familiarity with dependency graphs

---

## User Manual Card

**Component:** FeatureManager
**Primary use case:** Manage feature flags with complex interdependencies, automatic conflict detection, and observer-based notification of state changes
**Integration pattern:** Create `FeatureManager`, register features with dependencies via `addFeature()`, enable/disable features (auto-resolution handles dependencies), observe changes via callbacks
**Key API:** `FeatureManager`, `.addFeature()`, `.addRelationship()`, `.enable()`, `.disable()`, `.isEnabled()`, `.addObserver()`, `.addBatchObserver()`, `fromJson()`, `toJson()`
**std equivalent:** None
**Common mistakes:** Creating dependency cycles (detected at runtime, throws); enabling features without checking relationship constraints; modifying features from observer callbacks (reentrancy)
**Performance notes:** Feature lookup is O(1) hash map access. Dependency resolution is O(V+E) graph traversal. Observer notification is O(N) per observer. See `components/FeatureManager/results/` for current data

---
## Table of Contents

1. [Introduction](#introduction)
2. [The Problem Domain](#the-problem-domain)
3. [Why FeatureManager?](#why-featuremanager)
4. [Core Concepts](#core-concepts)
5. [Architecture Overview](#architecture-overview)
6. [Quick Start](#quick-start)
7. [API Reference](#api-reference)
8. [Callback Factory System](#callback-factory-system)
9. [Detailed Examples](#detailed-examples)
10. [Thread Safety](#thread-safety)
11. [Performance Characteristics](#performance-characteristics)
12. [Best Practices](#best-practices)
13. [Troubleshooting](#troubleshooting)
14. [Comparison with Other Libraries](#comparison-with-other-libraries)

---

## Introduction

FeatureManager is a C++20 header-only library for managing feature flags with complex dependencies, relationships, and validation rules. It provides a type-safe, high-performance solution for scenarios where features have interdependencies and need automatic resolution.

**License:** Header-only, zero external dependencies  
**C++ Standard:** C++20  
**Last Updated:** December 2025  
**Dependencies:** Expected.h, ConcurrencyPolicies.h, JsonLite.h, ValueGuard.h, Stringify.h, EnumPlus.h, FlatSet.h, Factory.h

## The Problem Domain

### Feature Flag Complexity in Real-World Systems

Feature flags (also called feature toggles) are an established technique for controlling functionality in software systems. However, as systems grow, feature flags become increasingly complex. Understanding these challenges helps explain why FeatureManager's architecture is necessary.

#### Problem 1: Interdependencies
```
Feature "Graphics_High" requires "GPU_Acceleration"
Feature "RayTracing" requires both "Graphics_High" AND "DX12_Support"
```

**Impact:** Without automatic dependency tracking, developers must manually enable required features in the correct order. In large codebases with 50+ features, this leads to:
- Integration bugs where features break because dependencies weren't enabled
- significant increases in QA time tracking down "works on my machine" issues where local configurations differ
- Production incidents when deployment scripts miss steps
- Developer frustration and context-switching overhead

**Real-world consequence:** A mobile game studio reported that a large fraction of their crash reports stemmed from features being enabled without their dependencies, particularly after configuration changes during A/B testing.

#### Problem 2: Conflicts
```
Feature "LowMemoryMode" conflicts with "HighQualityTextures"
Feature "BatteryOptimization" conflicts with "MaxPerformance"
```

**Impact:** Accidentally enabling conflicting features causes:
- Unpredictable behavior as features fight for resources
- Silent failures where one feature disables another's effects
- User complaints about inconsistent performance
- Difficult-to-reproduce bugs that only appear in specific feature combinations

**Real-world consequence:** In embedded systems, conflicting power management modes can cause devices to oscillate between states, draining batteries substantially faster than expected. Enterprise software often sees conflicts between "HighSecurity" and "FastMode" features that create security vulnerabilities.

#### Problem 3: Cascading Changes
```
Enabling "DeveloperMode" should automatically enable:
- "VerboseLogging"
- "DebugSymbols"
- "StackTraceCapture"
- "PerformanceCounters"
```

**Impact:** Without automatic propagation:
- System ends up in inconsistent states where some debug features are on, others off
- Developers waste time manually toggling 5-10 related features
- Documentation burden explaining the "correct" combination
- Support tickets from users who enabled one flag but not others

**Real-world consequence:** Debugging sessions take substantially longer when developers must remember to enable all related diagnostic features. In production, partial debug modes can leak sensitive information (e.g., verbose logging enabled but audit trails disabled).

#### Problem 4: Circular Dependencies
```
Feature A requires Feature B
Feature B requires Feature C
Feature C requires Feature A  ← CYCLE!
```

**Impact:** Circular dependencies are catastrophic:
- Stack overflow crashes in naive implementations
- Infinite loops consuming CPU
- System hangs during initialization
- Impossible-to-resolve configuration states

**Real-world consequence:** A graphics engine hit this when "OpenGL" required "ContextManager", which required "WindowSystem", which required "OpenGL" for initialization. The application crashed at startup with no clear diagnostic. Finding the cycle manually took 3 days.

#### Problem 5: Transactional Semantics
```
Try to enable ["FeatureA", "FeatureB", "FeatureC"]
If any fail (validation, conflicts, cycles), what happens?
```

**Impact:** Without transaction semantics:
- Partial failures leave the system in undefined states
- FeatureA and FeatureB might be enabled while FeatureC failed
- Rollback is manual and error-prone
- Race conditions in multi-threaded environments

**Real-world consequence:** During A/B test deployments, partial feature enable failures resulted in a measurable fraction of users receiving broken configurations that were hard to detect (some features on, others off). The fix required manually auditing and correcting thousands of user profiles.

#### Problem 6: Serialization of Validation Logic
```
Feature "GPU_Feature" has validation callback checking hardware
Save feature graph to JSON for persistence
Load from JSON... callbacks are LOST!
```

**Impact:** Traditional systems cannot serialize function pointers or lambdas:
- Feature configurations can be saved but validation logic is lost
- After restart, features are enabled without checking preconditions
- Cannot distribute configurations across systems
- Separation between config (serializable) and logic (code) is brittle

**Real-world consequence:** Medical device software had to maintain separate configuration files and validation rule engines, compounding complexity and causing synchronization bugs where configs allowed invalid states. A factory-based approach with string keys solves this by making validation logic reconstructible.

---

## Why FeatureManager?

FeatureManager solves these problems with a principled, graph-based architecture:

### 1. **Automatic Dependency Resolution**
When you enable a feature, FeatureManager automatically:
- Enables all required dependencies (transitive closure)
- Enables all implied features
- Checks for conflicts before making changes
- Validates the resulting state atomically

**How:** Uses recursive graph traversal with visited-set tracking (O(d × log n) where d = dependency depth, capped at 100).

### 2. **Cycle Detection with Clear Diagnostics**
Instead of stack overflow or infinite loops, you get:
```
"Circular dependency detected: Graphics_High -> RayTracing -> DX12_Support -> Graphics_High"
```

**How:** Maintains insertion-ordered path tracking during traversal. When a node is revisited, the exact cycle path is preserved for debugging.

### 3. **Transactional Batch Operations**
```cpp
manager.batchEnable({"A", "B", "C"});
// Either ALL succeed (including implicit dependencies)
// Or ALL are rolled back (no partial state)
```

**How:** Uses ValueGuard for scoped state changes. Records all modifications, commits only on success, or rolls back completely on any failure.

### 4. **Type-Safe Group States**
Define custom enums for domain-specific states:
```cpp
enum class RenderingState { Software, Basic, Enhanced, Ultra };
manager.addGroup<RenderingState>("Rendering", features, custom_computer);
```

**How:** Template-based state computation with user-provided functors. Groups track which features are enabled and compute composite states in O(n) where n = group size.

### 5. **Observer Pattern with RAII Lifecycle Management**
- Priority-ordered callbacks for deterministic notification order
- ID-based removal for reliable unregistration
- RAII helpers (`ScopedObserver`, `ScopedBatchObserver`) for automatic cleanup
- Batch observers receive all changed features in one callback

**How:** Observers stored with unique IDs. Notifications include all features that changed (including implicit dependencies). Scoped wrappers automatically unregister on destruction.

### 6. **Full Callback Serialization**
```cpp
// Register callback with key
factory.registerType("gpu.check", []() { return check_gpu(); });

// Use in feature
manager.addFeature("GPUFeature", "gpu.check");

// Serialize - callback key is saved
std::string json = manager.toJson();

// Deserialize - callback automatically restored!
auto restored = FeatureManager<>::fromJson(json);
```

**How:** FeatureCheckFactory singleton maps string keys to callback creator functions. Serialization stores keys, deserialization reconstructs callbacks via factory lookup. See [Callback Factory System](#callback-factory-system) for details.

### 7. **Performance Optimizations**
- O(1) average feature lookup using FastHashMap with string keys
- Cache-friendly iteration with FlatSet for relationships
- Lock-free fast paths where possible (configurable concurrency policies)
- Optimized rollback (only modified features, not full snapshots)

---

## Core Concepts

### Features

A **feature** is a named capability that can be enabled or disabled. Each feature can have:
- An enabled/disabled state (bool)
- A validation callback (FeatureCheck, optional)
- A callback key for serialization (string, optional)
- Relationships with other features (stored in adjacency lists)
- Membership in groups (for composite state tracking)

### Relationships

Four types of relationships connect features:

1. **Requires** - A requires B: Enabling A automatically enables B first. Directional.
2. **Implies** - A implies B: Enabling A automatically enables B afterward. Directional.
3. **Conflicts** - A conflicts with B: Both cannot be enabled simultaneously. Bidirectional (automatically adds reverse edge).
4. **MutuallyExclusive** - Group-level: Only one feature in the set can be enabled. Bidirectional among all members.

**Implementation Note:** Relationships are stored as `std::map<FeatureRelationship, std::set<std::string>>` per feature node.

### Groups

Features can be organized into **groups** with custom state computation. Groups track which features are enabled and compute an overall state using a user-provided functor.

**Example:** A "GraphicsQuality" group might compute a state enum (Low/Medium/High/Ultra) based on which graphics features are active.

### Validation Callbacks

**FeatureCheck** callbacks run when enabling features to validate preconditions:

```cpp
using FeatureCheck = std::function<Expected<void, std::string>()>;
```

Returns `Expected<void, string>` - success (empty Expected) or error message.

### Callback Factory

The **FeatureCheckFactory** is a global singleton that maps string keys to callback creator functions. This enables serialization of validation logic by storing/retrieving keys instead of function pointers. See dedicated section below.

---

## Architecture Overview

FeatureManager models features as a **directed graph**:
- **Nodes** = Features (with state, callback, metadata)
- **Edges** = Relationships (Requires, Implies, Conflicts, MutuallyExclusive)

### Graph Representation

```
Internally: std::map<std::string, FeatureNode>

FeatureNode {
    bool enabled;
    FeatureCheck check;
    std::string check_key;  // For serialization
    std::map<FeatureRelationship, std::set<std::string>> relationships;
}
```

### Core Algorithms

#### 1. Dependency Resolution (enable)
```
Pseudocode:
  visited = empty set
  path = empty stack (for cycle detection)
  
  enable_recursive(feature):
    if feature in path: REPORT CYCLE
    path.push(feature)
    visited.add(feature)
    
    for dependency in feature.requires:
      if not visited.has(dependency):
        enable_recursive(dependency)
    
    for implied in feature.implies:
      enable_recursive(implied)
    
    check_conflicts(feature)
    run_validation(feature)
    feature.enabled = true
    path.pop()
```

**Complexity:** O(d × log n) where d = max dependency depth (capped at 100 to prevent stack overflow)

#### 2. Conflict Detection
```
For each enabled feature:
  For each conflict edge:
    If target is also enabled: FAIL
```

**Complexity:** O(c) where c = number of conflict edges (typically small)

#### 3. Cycle Detection
Uses **insertion-order path tracking**. When a node is re-encountered during traversal, the path from initial visit to re-encounter is the cycle.

**Why insertion-order matters:** Ensures the reported cycle matches the actual dependency chain, not a random permutation.

#### 4. Transactional Batch Operations
```
batchEnable(features):
  snapshot = record current state
  try:
    for each feature:
      enable(feature)  // May enable dependencies
    commit
  catch error:
    rollback to snapshot
    rethrow
```

**Optimization:** Instead of full snapshots, tracks only modified features using FlatSet for O(log m) rollback where m = changes.

#### 5. Serialization with Factory
```
Serialization:
  For each feature:
    JSON["features"][name] = {
      "enabled": bool,
      "check_key": string (if present),
      "relationships": {...}
    }

Deserialization:
  For each JSON feature:
    node.enabled = JSON.enabled
    if JSON has check_key:
      node.check = factory.make(check_key)  // Reconstruct callback
    parse relationships
```

**Key insight:** Function pointers can't be serialized, but string keys can. Factory acts as a registry mapping keys → callbacks.

### Data Flow Diagram (ASCII)

```
User Code
    |
    v
[addFeature] ──→ Factory.make(key) ──→ FeatureCheck callback
    |                                        |
    v                                        v
FeatureManager.features                 Stored in node
    |
    v
[enable] ──→ Graph Traversal ──→ Dependency Resolution
    |            |                       |
    |            v                       v
    |      Cycle Detect           Run Validations
    |            |                       |
    v            v                       v
Success ←── Check Conflicts ←──── Notify Observers
```

### Thread Safety Model

Configurable via template parameter:
- **NoSynchronizationPolicy** (default): Single-threaded, no locks
- **MutexSynchronizationPolicy**: std::mutex, exclusive access
- **SharedMutexPolicy**: std::shared_mutex, concurrent reads

**Observer Safety:** Observers are called while holding the lock. Do not call FeatureManager methods from within observers (causes deadlock). Use flags or Implies/Requires relationships for cascading changes.

---

## Quick Start

### Basic Setup

```cpp
#include "FeatureManager.h"

using namespace fat_p;

int main() {
    // Create manager (single-threaded by default)
    FeatureManager<> manager;
    
    // Add features
    manager.addFeature("BasicGraphics");
    manager.addFeature("AdvancedGraphics");
    
    // Add relationship
    manager.addRelationship("AdvancedGraphics", 
                            FeatureRelationship::Requires, 
                            "BasicGraphics");
    
    // Enable feature (auto-enables dependencies)
    auto result = manager.enable("AdvancedGraphics");
    if (result) {
        // Both BasicGraphics and AdvancedGraphics are now enabled
        std::cout << "Enabled successfully\n";
    } else {
        std::cerr << "Error: " << result.error() << "\n";
    }
    
    return 0;
}
```

### With Validation Callbacks

For features requiring validation (e.g., hardware checks), use the factory system for serializability:

```cpp
// 1. Register callback in factory (do this once at startup)
auto& factory = getFeatureCheckFactory();
factory.registerType("gpu.check", []() -> FeatureCheck {
    return []() -> Expected<void, std::string> {
        if (!has_gpu()) {
            return unexpected("No GPU available");
        }
        return {};
    };
});

// 2. Add feature with callback key
manager.addFeature("GPUFeature", "gpu.check");

// 3. Enable (runs validation)
auto result = manager.enable("GPUFeature");
if (!result) {
    std::cerr << "Failed: " << result.error() << "\n";
}
```

### Serialization

Save and restore complete feature graphs including callbacks:

```cpp
// Save
std::string json = manager.toJson();
std::ofstream out("features.json");
out << json;

// Load (callbacks restored from factory)
std::ifstream in("features.json");
std::string loaded((std::istreambuf_iterator<char>(in)),
                   std::istreambuf_iterator<char>());
auto restored = FeatureManager<>::fromJson(loaded);
if (restored) {
    FeatureManager<> manager = std::move(*restored);
    // Ready to use - callbacks are automatically reconnected
}
```

**For full details on serialization, see [Callback Factory System](#callback-factory-system) below.**

---

## API Reference

### Feature Management

#### `addFeature()` - Direct Callback

Add a feature with a direct validation callback (not serializable).

**Signature:**
```cpp
Expected<void, std::string> addFeature(
    const std::string& name, 
    FeatureCheck check = nullptr
);
```

**Parameters:**
- `name`: Unique feature identifier
- `check`: Optional validation function

**Returns:**
- `Expected<void>` on success
- `unexpected(error_message)` if feature already exists

**Example:**
```cpp
// Simple feature
manager.addFeature("BasicFeature");

// With direct callback (NOT serializable)
manager.addFeature("GPUFeature", []() -> Expected<void, std::string> {
    if (!check_gpu_available()) {
        return unexpected("GPU not available");
    }
    return {};
});
```

**Warning:** Direct callbacks cannot be serialized. For persistence, use the factory-based version below.

---

#### `addFeature()` - Factory Key (Recommended)

Add a feature using a registered callback key (fully serializable).

**Signature:**
```cpp
Expected<void, std::string> addFeature(
    const std::string& name, 
    const std::string& check_key
);
```

**Parameters:**
- `name`: Unique feature identifier
- `check_key`: Key to look up callback in factory

**Returns:**
- `Expected<void>` on success
- `unexpected("Check key 'X' not found in factory")` if key not registered
- `unexpected("Feature already exists")` if feature exists

**Example:**
```cpp
// First, register the callback
auto& factory = getFeatureCheckFactory();
factory.registerType("hardware.gpu", []() -> FeatureCheck {
    return []() { return check_gpu(); };
});

// Then add feature with key
auto result = manager.addFeature("GPUFeature", "hardware.gpu");
if (!result) {
    std::cerr << result.error() << "\n";
}
```

**Benefits:**
- ✅ Full serialization support
- ✅ Callbacks automatically restored on load
- ✅ Module independence
- ✅ Type-safe validation

---

#### `addRelationship()`

Add a relationship between two features.

**Signature:**
```cpp
Expected<void, std::string> addRelationship(
    const std::string& from, 
    FeatureRelationship type, 
    const std::string& to
);
```

**Parameters:**
- `from`: Source feature name
- `type`: One of `Requires`, `Conflicts`, `Implies`, `MutuallyExclusive`
- `to`: Target feature name

**Returns:**
- `Expected<void>` on success
- `unexpected(error_message)` if features don't exist or relationship would create cycles

**Bidirectional Relationships:**
- `Conflicts` and `MutuallyExclusive` automatically add the reverse relationship
- `Requires` and `Implies` are directional only

**Example:**
```cpp
// A requires B (directional)
manager.addRelationship("FeatureA", FeatureRelationship::Requires, "FeatureB");

// A conflicts with B (bidirectional)
manager.addRelationship("FeatureA", FeatureRelationship::Conflicts, "FeatureB");
// Automatically adds: FeatureB conflicts with FeatureA
```

---

#### `enable()`

Enable a feature and all its dependencies.

**Signature:**
```cpp
Expected<void, std::string> enable(const std::string& name);
```

**Behavior:**
1. Checks for circular dependencies
2. Recursively enables all `Requires` dependencies
3. Recursively enables all `Implies` targets
4. Checks for conflicts with already-enabled features
5. Runs validation check if present
6. Notifies observers on success

**Returns:**
- `Expected<void>` on success
- `unexpected(error_message)` with detailed diagnostics including:
  - Cycle paths if circular dependency detected
  - Validation failure messages
  - Conflict descriptions

**Performance:** O(d × log n) where d = dependency depth (max 100), n = total features

**Example:**
```cpp
auto result = manager.enable("AdvancedFeature");
if (!result) {
    std::cerr << "Failed to enable: " << result.error() << "\n";
    // Error might be:
    // "Circular dependency detected: A -> B -> C -> A"
    // "Validation failed for 'GPU': No GPU available"
    // "Conflict: 'HighPerf' conflicts with already enabled 'PowerSave'"
}
```

---

#### `disable()`

Disable a feature.

**Signature:**
```cpp
Expected<void, std::string> disable(const std::string& name);
```

**Behavior:**
- Sets feature's enabled state to false
- Notifies observers
- Does **not** check if other features depend on this one

**Returns:**
- `Expected<void>` on success
- `unexpected("Feature not found")` if feature doesn't exist

**Note:** After disabling features, use `validate()` to check if the graph is still consistent.

**Example:**
```cpp
manager.disable("OptionalFeature");

// Ensure consistency
auto validation = manager.validate();
if (!validation) {
    std::cerr << "Warning: " << validation.error() << "\n";
}
```

---

#### `isEnabled()`

Check if a feature is enabled.

**Signature:**
```cpp
bool isEnabled(const std::string& name) const;
```

**Returns:** `true` if feature exists and is enabled, `false` otherwise

**Performance:** O(log n)

**Example:**
```cpp
if (manager.isEnabled("GPUAcceleration")) {
    use_gpu_path();
} else {
    use_cpu_path();
}
```

---

#### `validate()`

Validate the entire feature set for consistency.

**Signature:**
```cpp
Expected<void, std::string> validate();
```

**Checks:**
- All `Requires` relationships satisfied (if A requires B and A is enabled, B must be enabled)
- No conflicts between enabled features
- All `Implies` relationships satisfied (if A implies B and A is enabled, B must be enabled)
- All validation checks pass for enabled features
- No cycles in the dependency graph

**Returns:**
- `Expected<void>` if entire graph is consistent
- `unexpected(error_message)` with first inconsistency found

**Performance:** O(n × d × log n) where n = features, d = avg dependency depth

**Example:**
```cpp
// After manual modifications
manager.disable("CoreFeature");

auto result = manager.validate();
if (!result) {
    std::cerr << "Graph inconsistent: " << result.error() << "\n";
    // Might report: "'AdvancedFeature' requires 'CoreFeature' but it's disabled"
}
```

---

#### `batchEnable()`

Enable multiple features with transactional semantics.

**Signature:**
```cpp
Expected<void, std::string> batchEnable(
    const std::vector<std::string>& names
);
```

**Guarantees:**
1. **Atomicity:** All features (including dependencies) succeed or all fail
2. **Rollback:** Complete rollback on any failure
3. **Consistency:** Graph remains in valid state even if transaction fails

**Behavior:**
- Enables each feature in order
- Tracks all state changes (including implicit dependency enables)
- On failure, rolls back **all** changes, even dependencies enabled earlier
- Notifies observers only once per feature at end if successful

**Returns:**
- `Expected<void>` if all features and dependencies enabled
- `unexpected(error)` with detailed failure reason, graph unchanged

**Performance:** O(k × d × log n) where k = batch size

**Example:**
```cpp
std::vector<std::string> features = {"GPU", "HighQuality", "Shadows"};
auto result = manager.batchEnable(features);
if (!result) {
    std::cerr << "Batch failed: " << result.error() << "\n";
    // NO features are enabled, even if some succeeded initially
}
```

---

#### `batchDisable()`

Disable multiple features atomically with relationship validation.

**Signature:**
```cpp
Expected<void, std::string> batchDisable(
    const std::vector<std::string>& names
);
```

**Guarantees:**
1. **Atomicity:** All features disable successfully or none do
2. **Rollback:** Complete rollback on any validation failure
3. **Requires Validation:** Cannot disable a feature required by an enabled feature
4. **Implies Validation:** Cannot disable a feature implied by an enabled feature

**Behavior:**
- Validates all features exist first
- Tentatively disables all requested features
- Checks that no enabled feature requires any disabled feature
- Checks that no enabled feature implies any disabled feature
- On any failure, rolls back all changes
- Notifies observers for each feature that actually changed state

**Implies Relationship Semantics:**

If feature A implies feature B (meaning "when A is enabled, B must also be enabled"), then B cannot be disabled while A remains enabled. You must disable A first.

```cpp
manager.addRelationship("Premium", FeatureRelationship::Implies, "AllFeatures");
manager.enable("Premium");  // Both Premium and AllFeatures are now ON

// This FAILS - Premium implies AllFeatures
auto r1 = manager.batchDisable({"AllFeatures"});
// Error: "Cannot disable 'AllFeatures': implied by enabled feature 'Premium'. 
//         Disable 'Premium' first."

// This succeeds - disable the implier first
manager.disable("Premium");
auto r2 = manager.batchDisable({"AllFeatures"});  // OK
```

**Returns:**
- `Expected<void>` if all features successfully disabled
- `unexpected(error)` with detailed failure reason, graph unchanged

**Example:**
```cpp
// Scenario: Clean shutdown sequence
std::vector<std::string> to_disable = {"Networking", "Database", "Logging"};
auto result = manager.batchDisable(to_disable);
if (!result) {
    // Some feature depends on one we tried to disable
    std::cerr << "Cannot disable: " << result.error() << "\n";
}
```

---

#### `addObserver()`

Add an observer to be notified of feature state changes. Returns an ID for later removal.

**Signature:**
```cpp
ObserverId addObserver(
    FeatureObserver callback,
    int priority = 0
);
```

**Type Aliases:**
```cpp
using ObserverId = std::uint64_t;
using FeatureObserver = std::function<void(const std::string& featureName,
                                            bool newState,
                                            bool success)>;
```

**Parameters:**
- `callback`: Function called with (featureName, newState, success)
  - `featureName`: Name of feature that changed
  - `newState`: true if enabled, false if disabled
  - `success`: true if operation succeeded, false if rolled back
- `priority`: Higher values = called first (default 0)

**Returns:** `ObserverId` that can be used with `removeObserver()`

**Implicit Dependency Notifications:**

Observers are notified for **all** features that change state, not just the explicitly requested one. When enabling a feature that has dependencies via Requires or Implies relationships, observers receive separate notifications for each implicitly enabled feature.

```cpp
manager.addFeature("Core");
manager.addFeature("Module");
manager.addRelationship("Module", FeatureRelationship::Requires, "Core");

std::vector<std::string> notifications;
manager.addObserver([&](const std::string& name, bool enabled, bool) {
    if (enabled) notifications.push_back(name);
});

manager.enable("Module");  // Implicitly enables Core first

// notifications now contains: {"Core", "Module"}
// Observer was called twice - once for each changed feature
```

**Observer Contract:**
- **DO NOT** call FeatureManager methods inside observers (will deadlock)
- **DO NOT** perform long-running operations (blocks other observers)
- Observers are called **after** transaction completes
- Order determined by priority (highest first), then insertion order

**Example:**
```cpp
// Store the ID for later removal
ObserverId logObserver = manager.addObserver(
    [](const std::string& name, bool state, bool success) {
        if (success) {
            std::cout << "Feature " << name << (state ? " enabled" : " disabled") << "\n";
        }
    }, 
    /* priority */ 10
);

// Higher priority observer called first
ObserverId auditObserver = manager.addObserver(
    [](const std::string& name, bool state, bool success) {
        log_to_file(name, state);
    }, 
    /* priority */ 20
);

// Later, remove specific observers
manager.removeObserver(logObserver);
```

**Thread Safety:** Observer callbacks must be thread-safe if using concurrent policies.

---

#### `addBatchObserver()`

Add a batch observer that receives all changed features in a single callback.

**Signature:**
```cpp
ObserverId addBatchObserver(
    BatchObserver callback,
    int priority = 0
);
```

**Type Alias:**
```cpp
using BatchObserver = std::function<void(
    const std::string& requestedFeature,
    const std::vector<std::string>& allChanged,
    bool enabled,
    bool success
)>;
```

**Parameters:**
- `callback`: Function receiving:
  - `requestedFeature`: The feature explicitly requested by the user
  - `allChanged`: All features that changed state (includes implicit dependencies)
  - `enabled`: true if this was an enable operation, false for disable
  - `success`: true if operation succeeded
- `priority`: Higher values = called first (default 0)

**Returns:** `ObserverId` for later removal

**Use Cases:**
- Asset loading systems that need to know all features that changed
- UI updates that should refresh once after all changes
- Logging/analytics that need the complete picture
- Debugging dependency resolution

**Example:**
```cpp
manager.addBatchObserver([](const std::string& requested,
                               const std::vector<std::string>& allChanged,
                               bool enabled,
                               bool success) {
    if (!success) return;
    
    std::cout << "User requested: " << requested << "\n";
    std::cout << "Features that changed: ";
    for (const auto& f : allChanged) {
        std::cout << f << " ";
    }
    std::cout << "\n";
    
    // Load assets for all newly enabled features
    if (enabled) {
        for (const auto& feature : allChanged) {
            load_feature_assets(feature);
        }
    }
});

manager.enable("AdvancedMode");
// Output:
// User requested: AdvancedMode
// Features that changed: BasicMode CoreModule AdvancedMode
```

---

#### `removeObserver()`

Remove an observer by its ID.

**Signature:**
```cpp
bool removeObserver(ObserverId id);
```

**Parameters:**
- `id`: The `ObserverId` returned by `addObserver()` or `addBatchObserver()`

**Returns:** `true` if an observer with that ID was found and removed, `false` otherwise

**Note:** Works for both regular observers and batch observers. The ID uniquely identifies the observer regardless of type.

**Example:**
```cpp
// Add and later remove
ObserverId id = manager.addObserver([](auto...) { /* ... */ });

// ... later ...
bool removed = manager.removeObserver(id);
if (removed) {
    std::cout << "Observer successfully removed\n";
}

// Removing again returns false
bool removed_again = manager.removeObserver(id);  // false
```

---

#### `clearObservers()`

Remove all observers (both regular and batch).

**Signature:**
```cpp
void clearObservers();
```

**Use Cases:**
- Resetting feature manager to clean state
- Test teardown
- Transitioning between application phases

**Example:**
```cpp
// Add several observers during initialization
manager.addObserver(logging_observer);
manager.addObserver(metrics_observer);
manager.addBatchObserver(ui_update_observer);

// Later, during shutdown or reset
manager.clearObservers();  // All observers removed
```

---

#### `ScopedObserver`

RAII helper for automatic observer registration/unregistration.

**Declaration:**
```cpp
class FeatureManager::ScopedObserver {
public:
    ScopedObserver(FeatureManager& manager, FeatureObserver callback, int priority = 0);
    ~ScopedObserver();  // Automatically calls removeObserver()
    
    // Move-only (not copyable)
    ScopedObserver(ScopedObserver&& other) noexcept;
    ScopedObserver& operator=(ScopedObserver&& other) noexcept;
    
    ObserverId id() const;      // Get the observer ID
    ObserverId release();       // Release ownership without unregistering
};
```

**Purpose:**

Ensures observers are automatically unregistered when the scope ends, preventing:
- Memory leaks from accumulated observers
- Dangling references if the observer captures `this` from a destroyed object
- Manual cleanup bookkeeping

**Example - Basic RAII:**
```cpp
{
    FeatureManager<>::ScopedObserver observer(manager,
        [](const std::string& name, bool enabled, bool) {
            std::cout << name << " changed to " << enabled << "\n";
        });
    
    manager.enable("Feature1");  // Observer is called
    manager.enable("Feature2");  // Observer is called
    
}  // Observer automatically removed here

manager.enable("Feature3");  // Observer is NOT called
```

**Example - Member Observer Pattern:**
```cpp
class FeatureController {
    FeatureManager<>& manager_;
    FeatureManager<>::ScopedObserver observer_;
    
public:
    FeatureController(FeatureManager<>& m)
        : manager_(m)
        , observer_(m, [this](auto name, auto enabled, auto) {
            this->on_feature_change(name, enabled);  // Safe - observer removed in dtor
          })
    {}
    
    ~FeatureController() = default;  // observer_ automatically cleaned up
    
private:
    void on_feature_change(const std::string& name, bool enabled) {
        // Handle change...
    }
};
```

**Example - Conditional Observation:**
```cpp
std::optional<FeatureManager<>::ScopedObserver> observer;

if (debug_mode) {
    observer.emplace(manager, [](auto...) { /* debug logging */ });
}

// ... observer active only if debug_mode was true ...

observer.reset();  // Explicitly remove early if needed
```

**Example - Transfer Ownership:**
```cpp
FeatureManager<>::ScopedObserver createObserver(FeatureManager<>& m) {
    return FeatureManager<>::ScopedObserver(m, [](auto...) { /* ... */ });
}

auto obs = createObserver(manager);  // Ownership transferred via move
```

---

#### `ScopedBatchObserver`

RAII helper for batch observers. Same semantics as `ScopedObserver`.

**Declaration:**
```cpp
class FeatureManager::ScopedBatchObserver {
public:
    ScopedBatchObserver(FeatureManager& manager, BatchObserver callback, int priority = 0);
    ~ScopedBatchObserver();
    
    ScopedBatchObserver(ScopedBatchObserver&& other) noexcept;
    ScopedBatchObserver& operator=(ScopedBatchObserver&& other) noexcept;
    
    ObserverId id() const;
    ObserverId release();
};
```

**Example:**
```cpp
{
    FeatureManager<>::ScopedBatchObserver batch_obs(manager,
        [](auto requested, auto allChanged, auto enabled, auto success) {
            if (success && enabled) {
                reload_ui_for_features(allChanged);
            }
        });
    
    manager.enable("AdvancedMode");  // Batch observer called once with all changes
    
}  // Batch observer automatically removed
```

---

#### `addGroup()`

Add a feature group with custom state computation.

**Signature:**
```cpp
template<typename StateType>
Expected<void, std::string> addGroup(
    const std::string& name,
    const std::vector<std::string>& featureNames,
    std::function<StateType(const std::set<std::string>&)> stateComputer
);
```

**Parameters:**
- `name`: Unique group identifier
- `featureNames`: Features belonging to this group
- `stateComputer`: Function that computes group state from set of enabled features

**Returns:**
- `Expected<void>` on success
- `unexpected(error)` if group exists or features don't exist

**Example:**
```cpp
enum class GraphicsQuality { Low, Medium, High, Ultra };

manager.addGroup<GraphicsQuality>(
    "Graphics",
    {"BasicGraphics", "Textures", "Shadows", "RayTracing"},
    [](const std::set<std::string>& enabled) -> GraphicsQuality {
        if (enabled.count("RayTracing")) return GraphicsQuality::Ultra;
        if (enabled.count("Shadows")) return GraphicsQuality::High;
        if (enabled.count("Textures")) return GraphicsQuality::Medium;
        return GraphicsQuality::Low;
    }
);

// Query group state
auto state = manager.getGroupState<GraphicsQuality>("Graphics");
```

---

#### `getGroupState()`

Get the current computed state of a group.

**Signature:**
```cpp
template<typename StateType>
Expected<StateType, std::string> getGroupState(
    const std::string& groupName
);
```

**Returns:**
- `Expected<StateType>` with computed state
- `unexpected(error)` if group doesn't exist or type mismatch

---

#### `toDot()`

Export graph to GraphViz DOT format for visualization.

**Signature:**
```cpp
std::string toDot() const;
```

**Returns:** String containing DOT graph description

**Example:**
```cpp
std::string dot = manager.toDot();
std::ofstream out("features.dot");
out << dot;
// Convert to image: dot -Tpng features.dot -o features.png
```

**Output Format:**
- Nodes: Features (green=enabled, red=disabled)
- Edges: Relationships (black=Requires, blue=Implies, red=Conflicts)

---

### Serialization

#### `toJson()`

Export state to JSON (includes callback keys).

**Signature:**
```cpp
std::string toJson() const;
```

**Returns:** JSON string representing entire feature graph

**Format:**
```json
{
  "features": {
    "GPUFeature": {
      "enabled": true,
      "check_key": "hardware.gpu",
      "relationships": {
        "Requires": ["BasicFeature"],
        "Conflicts": ["SoftwareRenderer"]
      }
    },
    "BasicFeature": {
      "enabled": true
    }
  }
}
```

**Example:**
```cpp
std::string json = manager.toJson();
save_to_file("config.json", json);
```

**Note:** Only callback **keys** are serialized, not the callbacks themselves. See [Callback Factory System](#callback-factory-system).

---

#### `fromJson()`

Import state from JSON (restores callbacks from factory).

**Signature:**
```cpp
static Expected<FeatureManager, std::string> fromJson(
    const std::string& jsonStr
);
```

**Returns:**
- `Expected<FeatureManager>` with restored state
- `unexpected(error)` if JSON invalid or callback keys not found

**Example:**
```cpp
std::string json = load_from_file("config.json");
auto result = FeatureManager<>::fromJson(json);
if (result) {
    FeatureManager<> manager = std::move(*result);
    // Callbacks automatically restored via factory lookups
} else {
    std::cerr << "Failed to load: " << result.error() << "\n";
}
```

**Prerequisites:** Callback factory must be initialized with all keys referenced in JSON **before** calling `fromJson`.

---

## Callback Factory System

The callback factory system is the mechanism that enables full serialization of feature graphs including validation logic. This section is the definitive reference for the factory system.

### The Serialization Problem

**Challenge:** Function pointers and lambda captures cannot be serialized to JSON.

```cpp
// This works at runtime:
manager.addFeature("GPU", []() { return check_gpu(); });

// But toJson() cannot save this lambda:
std::string json = manager.toJson();  // Lambda is lost!

// After fromJson(), the callback is gone:
auto restored = FeatureManager<>::fromJson(json);
// "GPU" feature exists but has no validation callback
```

### The Factory Solution

**Approach:** Register callbacks with string keys. Serialize keys instead of callbacks. Reconstruct callbacks via factory lookup.

```
Runtime:    Key String ──→ Factory ──→ Callback Function
            "gpu.check"      │           check_gpu()
                             │
Serialize:  JSON["check_key"] = "gpu.check"
                             │
Load:       "gpu.check" ──→ Factory ──→ Callback Reconstructed!
```

### Global Factory

Access the singleton factory instance:

```cpp
auto& factory = fat_p::getFeatureCheckFactory();
```

**Type:** `FeatureCheckFactory` (alias for `SimpleFactory<std::string, FeatureCheck>`)

### Registration API

#### `registerType()`

Register a callback creator with a string key.

**Signature:**
```cpp
bool registerType(
    const std::string& key, 
    std::function<FeatureCheck()> creator
);
```

**Parameters:**
- `key`: Unique string identifier (e.g., "hardware.gpu")
- `creator`: Function that returns a `FeatureCheck` callback

**Returns:** `true` if registered, `false` if key already exists

**Example:**
```cpp
auto& factory = getFeatureCheckFactory();

bool success = factory.registerType("gpu.check", []() -> FeatureCheck {
    return []() -> Expected<void, std::string> {
        if (!has_gpu()) return unexpected("No GPU");
        return {};
    };
});

if (!success) {
    std::cerr << "Key 'gpu.check' already registered\n";
}
```

#### `unregisterType()`

Remove a registered callback.

**Signature:**
```cpp
bool unregisterType(const std::string& key);
```

**Returns:** `true` if key was found and removed

**Use Case:** Plugin unloading, dynamic module management

#### `make()`

Create a callback from a registered key.

**Signature:**
```cpp
Expected<FeatureCheck, std::string> make(const std::string& key);
```

**Returns:**
- `Expected<FeatureCheck>` with callback
- `unexpected(error)` if key not found

**Example:**
```cpp
auto result = factory.make("gpu.check");
if (result) {
    FeatureCheck callback = *result;
    auto validation = callback();  // Run check
}
```

#### `hasType()`

Check if a key is registered.

**Signature:**
```cpp
bool hasType(const std::string& key) const;
```

**Example:**
```cpp
if (!factory.hasType("gpu.check")) {
    factory.registerType("gpu.check", /* ... */);
}
```

#### `clear()`

Remove all registrations.

**Signature:**
```cpp
void clear();
```

**Warning:** Use cautiously - clears all registered callbacks including those from other modules.

### Registration Patterns

#### Pattern 1: Module-Based Registration

Each module independently registers its callbacks during initialization:

```cpp
// graphics_module.cpp
namespace graphics {
    void init() {
        auto& factory = getFeatureCheckFactory();
        
        factory.registerType("graphics.opengl", []() -> FeatureCheck {
            return []() { return check_opengl(); };
        });
        
        factory.registerType("graphics.vulkan", []() -> FeatureCheck {
            return []() { return check_vulkan(); };
        });
        
        factory.registerType("graphics.directx", []() -> FeatureCheck {
            return []() { return check_directx(); };
        });
    }
}

// audio_module.cpp
namespace audio {
    void init() {
        auto& factory = getFeatureCheckFactory();
        
        factory.registerType("audio.wasapi", []() -> FeatureCheck {
            return []() { return check_wasapi(); };
        });
        
        factory.registerType("audio.openal", []() -> FeatureCheck {
            return []() { return check_openal(); };
        });
    }
}

// main.cpp
int main() {
    // Initialize all modules BEFORE loading features
    graphics::init();
    audio::init();
    network::init();
    
    // Now load feature configuration
    auto manager = FeatureManager<>::fromJson(load_config());
    // All callbacks automatically restored via factory
}
```

**Benefits:**
- Modules don't need to know about each other
- Registration happens once at startup
- Clear ownership of callback keys per module

#### Pattern 2: RAII Registration

Automatic cleanup when objects are destroyed:

```cpp
class GraphicsPlugin {
public:
    GraphicsPlugin(const std::string& name) : name_(name) {
        registration_ = std::make_unique<FeatureCheckRegistration>(
            "plugin.graphics." + name_,
            [this]() -> FeatureCheck {
                return [this]() { return this->check_compatibility(); };
            }
        );
    }
    
    // Destructor automatically unregisters callback
    ~GraphicsPlugin() = default;
    
private:
    Expected<void, std::string> check_compatibility() {
        // Plugin-specific validation
        if (!is_compatible()) {
            return unexpected("Plugin incompatible");
        }
        return {};
    }
    
    std::string name_;
    std::unique_ptr<FeatureCheckRegistration> registration_;
};

// Usage:
{
    GraphicsPlugin opengl("opengl");
    GraphicsPlugin vulkan("vulkan");
    
    FeatureManager<> manager;
    manager.addFeature("OpenGL", "plugin.graphics.opengl");
    manager.addFeature("Vulkan", "plugin.graphics.vulkan");
    
    // Use features...
    
} // Plugins destroyed → callbacks automatically unregistered
```

**Benefits:**
- Exception-safe cleanup
- No manual unregister calls
- Ideal for dynamic plugin systems

#### Pattern 3: Member Function Callbacks

Capture class instances to call member functions:

```cpp
class HardwareMonitor {
public:
    void register_checks() {
        auto& factory = getFeatureCheckFactory();
        
        // Capture 'this' to call member functions
        factory.registerType("hardware.gpu", [this]() -> FeatureCheck {
            return [this]() { return this->check_gpu_available(); };
        });
        
        factory.registerType("hardware.memory", [this]() -> FeatureCheck {
            return [this]() { return this->check_memory_sufficient(); };
        });
    }
    
private:
    Expected<void, std::string> check_gpu_available() {
        if (!gpu_present_) {
            return unexpected("No GPU detected");
        }
        if (gpu_memory_mb_ < 1024) {
            return unexpected("GPU has insufficient memory");
        }
        return {};
    }
    
    Expected<void, std::string> check_memory_sufficient() {
        if (system_memory_mb_ < min_memory_mb_) {
            return unexpected("Insufficient system memory");
        }
        return {};
    }
    
    bool gpu_present_ = true;
    int gpu_memory_mb_ = 0;
    int system_memory_mb_ = 0;
    int min_memory_mb_ = 2048;
};

// Usage:
auto monitor = std::make_shared<HardwareMonitor>();
monitor->register_checks();

FeatureManager<> manager;
manager.addFeature("GPUAcceleration", "hardware.gpu");
manager.addFeature("HighQuality", "hardware.memory");
```

**Warning:** Ensure the captured object (`this`) outlives the factory registration. Use `shared_ptr` for safety:

```cpp
factory.registerType("key", [monitor]() -> FeatureCheck {
    return [monitor]() { return monitor->check(); };
});
```

#### Pattern 4: Conditional Validation

Capture configuration or state for dynamic validation:

```cpp
struct Configuration {
    int min_memory_mb = 2048;
    bool require_gpu = true;
    std::string required_os_version = "10.0";
};

Configuration config = load_configuration();

auto& factory = getFeatureCheckFactory();

factory.registerType("system.requirements", [config]() -> FeatureCheck {
    return [config]() -> Expected<void, std::string> {
        if (get_system_memory() < config.min_memory_mb) {
            return unexpected("Insufficient memory");
        }
        if (config.require_gpu && !has_gpu()) {
            return unexpected("GPU required but not found");
        }
        if (get_os_version() < config.required_os_version) {
            return unexpected("OS version too old");
        }
        return {};
    };
});
```

### Key Naming Conventions

Use **hierarchical dot notation** for organization:

```cpp
// ✅ GOOD - Clear hierarchy
"graphics.renderer.opengl"
"graphics.renderer.vulkan"
"graphics.renderer.directx12"
"audio.output.wasapi"
"audio.output.openal"
"audio.input.microphone"
"network.protocol.tcp"
"network.protocol.udp"
"system.cpu.avx2"
"system.cpu.sse4"

// ❌ BAD - Unclear, unmaintainable
"check1"
"check2"
"gpuvalidate"
"test"
"callback_func"
```

**Recommendation:** Use constants or enums:

```cpp
namespace CheckKeys {
    constexpr const char* GRAPHICS_OPENGL = "graphics.renderer.opengl";
    constexpr const char* AUDIO_WASAPI = "audio.output.wasapi";
}

factory.registerType(CheckKeys::GRAPHICS_OPENGL, /* ... */);
manager.addFeature("OpenGL", CheckKeys::GRAPHICS_OPENGL);
```

### Complete Serialization Workflow

```cpp
// ═══════════════════════════════════════
// STEP 1: Register callbacks at startup
// ═══════════════════════════════════════
void initialize_system() {
    auto& factory = getFeatureCheckFactory();
    
    factory.registerType("gpu.check", []() -> FeatureCheck {
        return []() { return has_gpu() ? Expected<void, std::string>{} 
                                       : unexpected("No GPU"); };
    });
    
    factory.registerType("memory.check", []() -> FeatureCheck {
        return []() { return get_memory() >= 2048 ? Expected<void, std::string>{} 
                                                  : unexpected("Low memory"); };
    });
}

// ═══════════════════════════════════════
// STEP 2: Create features using keys
// ═══════════════════════════════════════
void setup_features() {
    FeatureManager<> manager;
    
    manager.addFeature("GPUAcceleration", "gpu.check");
    manager.addFeature("HighQuality", "memory.check");
    manager.addRelationship("HighQuality", 
                            FeatureRelationship::Requires, 
                            "GPUAcceleration");
    
    manager.enable("HighQuality");
    
    // Save configuration
    std::string json = manager.toJson();
    save_to_file("features.json", json);
}

// ═══════════════════════════════════════
// STEP 3: Load configuration later
// ═══════════════════════════════════════
void load_features() {
    // CRITICAL: Initialize factory BEFORE loading
    initialize_system();
    
    // Load JSON
    std::string json = load_from_file("features.json");
    auto result = FeatureManager<>::fromJson(json);
    
    if (result) {
        FeatureManager<> manager = std::move(*result);
        // Callbacks automatically restored via factory!
        
        // Verify: validation still works
        auto enable_result = manager.enable("GPUAcceleration");
        if (!enable_result) {
            std::cerr << "Validation failed: " << enable_result.error() << "\n";
        }
    } else {
        std::cerr << "Load failed: " << result.error() << "\n";
    }
}
```

### Thread Safety

The default `FeatureCheckFactory` uses `SimpleFactory` which is **not** thread-safe.

**For multi-threaded use:**

```cpp
// Option 1: Change factory type in FeatureManager.h
using FeatureCheckFactory = ThreadSafeFactory<std::string, FeatureCheck>;

// Option 2: Guard registrations manually
std::mutex factory_mutex;
{
    std::lock_guard lock(factory_mutex);
    factory.registerType("key", /* ... */);
}
```

**Note:** Once registered, factory lookups during `fromJson()` are typically safe as long as no concurrent `registerType`/`unregisterType` calls occur.

### Best Practices

1. **Register Early:** Initialize factory before any `fromJson()` calls
2. **Use Namespaces:** Organize keys hierarchically (`module.subsystem.check`)
3. **Document Keys:** Create constants/enums for key strings
4. **Check Success:** Verify `registerType()` returns true
5. **RAII for Plugins:** Use `FeatureCheckRegistration` for automatic cleanup
6. **Avoid Duplication:** Check `hasType()` before re-registering
7. **Test Roundtrip:** Verify save→load preserves behavior
8. **Handle Missing:** Gracefully handle missing keys during load

### Common Pitfalls

#### Pitfall 1: Loading Before Registration

```cpp
// ❌ BAD
auto manager = FeatureManager<>::fromJson(json);  // Keys not found!
initialize_factory();  // Too late

// ✅ GOOD
initialize_factory();  // Register first
auto manager = FeatureManager<>::fromJson(json);
```

#### Pitfall 2: Lifetime Issues

```cpp
// ❌ BAD - 'this' becomes dangling
class Plugin {
    void init() {
        factory.registerType("key", [this]() -> FeatureCheck {
            return [this]() { return validate(); };
        });
    }
};
Plugin* plugin = new Plugin();
plugin->init();
delete plugin;  // Callback now has dangling 'this'!

// ✅ GOOD - Use shared_ptr
auto plugin = std::make_shared<Plugin>();
factory.registerType("key", [plugin]() -> FeatureCheck {
    return [plugin]() { return plugin->validate(); };
});
```

#### Pitfall 3: Duplicate Keys

```cpp
// ❌ BAD - Silent failure
factory.registerType("key", callback1);
factory.registerType("key", callback2);  // Returns false, callback1 remains

// ✅ GOOD - Check return value
if (!factory.registerType("key", callback)) {
    std::cerr << "Key already registered\n";
}
```

### Custom Group Serialization Limitations

Custom `StateEnum` types and `StateComputer` functors are **not serialized**. This is a fundamental C++ limitation—lambdas and `std::function` objects cannot be portably persisted.

After `fromJson()`, custom groups revert to the default `FeatureGroupState`. Calling `getGroupState<CustomEnum>("group")` on a deserialized manager returns "Type mismatch".

**Workaround:** Re-register custom groups after deserialization:

```cpp
auto restored = FeatureManager<>::fromJson(json);
if (restored) {
    // Group exists with default state type - replace with custom
    restored->addGroup<NetworkState>("Network", {"WiFi", "Bluetooth"}, 
                                       network_state_computer);
}
```

For advanced use cases requiring persistent custom groups, implement a `StateComputer` factory following the same pattern as `FeatureCheckFactory`:

```cpp
// Define factory type
using NetworkComputerFactory = SimpleFactory<std::string, StateComputer<NetworkState>>;

// Register at startup
network_factory.registerType("network.standard", []() {
    return [](const std::set<std::string>& features, 
              const std::function<bool(const std::string&)>& isEnabled) {
        // Custom state computation logic
        return NetworkState::Connected;
    };
});

// After deserializing, reconstruct with factory
auto computer = network_factory.make("network.standard");
if (computer) {
    restored->addGroup<NetworkState>("Network", {"WiFi", "Bluetooth"}, *computer);
}
```

**Note:** This factory approach keeps the core library lightweight while enabling persistence for users who need it.

---

## Detailed Examples

### Example 1: Game Graphics Settings with Serialization

```cpp
#include "FeatureManager.h"
#include <iostream>
#include <fstream>

// Simulated hardware checks
bool detect_gpu() { return true; }
int get_free_memory() { return 4096; }  // MB

void init_graphics_checks() {
    auto& factory = fat_p::getFeatureCheckFactory();

    factory.registerType("gpu.available", []() -> fat_p::feature::FeatureCheck {
        return []() -> fat_p::Expected<void, std::string> {
            if (!detect_gpu()) {
                return fat_p::unexpected("No GPU detected");
            }
            return {};
        };
    });

    factory.registerType("memory.sufficient", []() -> fat_p::feature::FeatureCheck {
        return []() -> fat_p::Expected<void, std::string> {
            if (get_free_memory() < 2048) {
                return fat_p::unexpected("Need at least 2GB RAM");
            }
            return {};
        };
    });
}

int main() {
    // Initialize factory first
    init_graphics_checks();
    
    // Create feature graph
    fat_p::feature::FeatureManager<> manager;
    
    manager.addFeature("BasicGraphics");
    manager.addFeature("GPU", "gpu.available");
    manager.addFeature("HighQuality", "memory.sufficient");
    
    manager.addRelationship("GPU", 
                            fat_p::feature::FeatureRelationship::Requires, 
                            "BasicGraphics");
    manager.addRelationship("HighQuality", 
                            fat_p::feature::FeatureRelationship::Requires, 
                            "GPU");
    
    // Try to enable high quality (validates GPU and memory)
    auto result = manager.enable("HighQuality");
    if (result) {
        std::cout << "High quality graphics enabled\n";
    } else {
        std::cout << "Falling back to basic graphics: " 
                  << result.error() << "\n";
        manager.enable("BasicGraphics");
    }
    
    // Save configuration
    std::string json = manager.toJson();
    std::ofstream out("game_settings.json");
    out << json;
    out.close();
    
    std::cout << "Settings saved\n";
    
    // ═══════════════════════════════════════
    // Later (maybe after restart)...
    // ═══════════════════════════════════════
    
    // Re-initialize factory (critical!)
    init_graphics_checks();
    
    // Load settings
    std::ifstream in("game_settings.json");
    std::string loaded((std::istreambuf_iterator<char>(in)),
                       std::istreambuf_iterator<char>());
    
    auto restored = fat_p::feature::FeatureManager<>::fromJson(loaded);
    if (restored) {
        std::cout << "Settings restored - callbacks work!\n";
        
        // Validation still works after load
        if (restored->isEnabled("HighQuality")) {
            std::cout << "High quality mode active\n";
        }
    }
    
    return 0;
}
```

### Example 2: Plugin System with RAII

```cpp
#include "FeatureManager.h"
#include <memory>

class GraphicsPlugin {
public:
    GraphicsPlugin(const std::string& name) 
        : name_(name) {
        
        // RAII registration - automatically unregisters on destruction
        registration_ = std::make_unique<fat_p::feature::FeatureCheckRegistration>(
            "plugin.graphics." + name_,
            [this]() -> fat_p::feature::FeatureCheck {
                return [this]() { return check_compatibility(); };
            }
        );
        
        std::cout << "Plugin '" << name_ << "' registered\n";
    }
    
    ~GraphicsPlugin() {
        std::cout << "Plugin '" << name_ << "' unregistered\n";
    }
    
private:
    fat_p::Expected<void, std::string> check_compatibility() {
        // Plugin-specific validation
        if (name_ == "vulkan" && !has_vulkan_support()) {
            return fat_p::unexpected("Vulkan not supported");
        }
        return {};
    }
    
    bool has_vulkan_support() { return true; }
    
    std::string name_;
    std::unique_ptr<fat_p::feature::FeatureCheckRegistration> registration_;
};

int main() {
    fat_p::feature::FeatureManager<> manager;
    
    {
        // Create plugins (automatically register callbacks)
        GraphicsPlugin opengl("opengl");
        GraphicsPlugin vulkan("vulkan");
        GraphicsPlugin directx("directx");
        
        // Add features using registered keys
        manager.addFeature("OpenGL", "plugin.graphics.opengl");
        manager.addFeature("Vulkan", "plugin.graphics.vulkan");
        manager.addFeature("DirectX", "plugin.graphics.directx");
        
        manager.enable("Vulkan");
        
        // Use features...
        
    } // Plugins destroyed → callbacks automatically unregistered
    
    // Attempting to add features with unregistered keys will fail
    auto result = manager.addFeature("NewPlugin", "plugin.graphics.opengl");
    if (!result) {
        std::cout << "Expected: " << result.error() << "\n";
    }
    
    return 0;
}
```

### Example 3: Multi-Module Application

```cpp
#include "FeatureManager.h"

// ═══════════════════════════════════════
// Network Module
// ═══════════════════════════════════════
namespace network {
    bool check_ipv4() { return true; }
    bool check_ipv6() { return false; }
    
    void init() {
        auto& factory = fat_p::getFeatureCheckFactory();
        
        factory.registerType("network.ipv4", []() -> fat_p::feature::FeatureCheck {
            return []() {
                return check_ipv4() ? fat_p::Expected<void, std::string>{} 
                                    : fat_p::unexpected("IPv4 not available");
            };
        });
        
        factory.registerType("network.ipv6", []() -> fat_p::feature::FeatureCheck {
            return []() {
                return check_ipv6() ? fat_p::Expected<void, std::string>{} 
                                    : fat_p::unexpected("IPv6 not available");
            };
        });
    }
}

// ═══════════════════════════════════════
// Graphics Module
// ═══════════════════════════════════════
namespace graphics {
    bool check_opengl() { return true; }
    bool check_vulkan() { return true; }
    
    void init() {
        auto& factory = fat_p::getFeatureCheckFactory();
        
        factory.registerType("graphics.opengl", []() -> fat_p::feature::FeatureCheck {
            return []() {
                return check_opengl() ? fat_p::Expected<void, std::string>{} 
                                      : fat_p::unexpected("OpenGL not supported");
            };
        });
        
        factory.registerType("graphics.vulkan", []() -> fat_p::feature::FeatureCheck {
            return []() {
                return check_vulkan() ? fat_p::Expected<void, std::string>{} 
                                      : fat_p::unexpected("Vulkan not supported");
            };
        });
    }
}

// ═══════════════════════════════════════
// Audio Module
// ═══════════════════════════════════════
namespace audio {
    bool check_wasapi() { return true; }
    
    void init() {
        auto& factory = fat_p::getFeatureCheckFactory();
        
        factory.registerType("audio.wasapi", []() -> fat_p::feature::FeatureCheck {
            return []() {
                return check_wasapi() ? fat_p::Expected<void, std::string>{} 
                                      : fat_p::unexpected("WASAPI not available");
            };
        });
    }
}

// ═══════════════════════════════════════
// Main Application
// ═══════════════════════════════════════
int main() {
    // Initialize all modules (each registers independently)
    network::init();
    graphics::init();
    audio::init();
    
    // Modules don't know about each other!
    
    // Create features from all modules
    fat_p::feature::FeatureManager<> manager;
    
    manager.addFeature("IPv4", "network.ipv4");
    manager.addFeature("IPv6", "network.ipv6");
    manager.addFeature("OpenGL", "graphics.opengl");
    manager.addFeature("Vulkan", "graphics.vulkan");
    manager.addFeature("WASAPI", "audio.wasapi");
    
    // Add cross-module dependencies
    manager.addRelationship("Vulkan", 
                            fat_p::feature::FeatureRelationship::Requires, 
                            "IPv4");
    
    // Enable features
    auto result = manager.enable("Vulkan");
    if (result) {
        std::cout << "Vulkan enabled (IPv4 auto-enabled)\n";
    }
    
    // Serialize entire graph
    std::string json = manager.toJson();
    save_to_file("app_config.json", json);
    
    return 0;
}
```

### Example 4: Custom Group States

```cpp
#include "FeatureManager.h"
#include <iostream>

enum class GraphicsQuality { Low, Medium, High, Ultra };
enum class NetworkMode { Offline, Local, Online };

int main() {
    fat_p::feature::FeatureManager<> manager;
    
    // Add graphics features
    manager.addFeature("BasicGraphics");
    manager.addFeature("Textures");
    manager.addFeature("Shadows");
    manager.addFeature("RayTracing");
    
    // Define graphics quality group
    manager.addGroup<GraphicsQuality>(
        "Graphics",
        {"BasicGraphics", "Textures", "Shadows", "RayTracing"},
        [](const std::set<std::string>& enabled) -> GraphicsQuality {
            if (enabled.count("RayTracing")) return GraphicsQuality::Ultra;
            if (enabled.count("Shadows")) return GraphicsQuality::High;
            if (enabled.count("Textures")) return GraphicsQuality::Medium;
            return GraphicsQuality::Low;
        }
    );
    
    // Add network features
    manager.addFeature("LocalMultiplayer");
    manager.addFeature("OnlineMultiplayer");
    
    // Define network mode group
    manager.addGroup<NetworkMode>(
        "Network",
        {"LocalMultiplayer", "OnlineMultiplayer"},
        [](const std::set<std::string>& enabled) -> NetworkMode {
            if (enabled.count("OnlineMultiplayer")) return NetworkMode::Online;
            if (enabled.count("LocalMultiplayer")) return NetworkMode::Local;
            return NetworkMode::Offline;
        }
    );
    
    // Change settings and query group states
    manager.enable("Shadows");
    
    auto gfx_state = manager.getGroupState<GraphicsQuality>("Graphics");
    if (gfx_state) {
        std::cout << "Graphics quality: ";
        switch (*gfx_state) {
            case GraphicsQuality::Ultra: std::cout << "Ultra\n"; break;
            case GraphicsQuality::High: std::cout << "High\n"; break;
            case GraphicsQuality::Medium: std::cout << "Medium\n"; break;
            case GraphicsQuality::Low: std::cout << "Low\n"; break;
        }
    }
    
    manager.enable("OnlineMultiplayer");
    
    auto net_state = manager.getGroupState<NetworkMode>("Network");
    if (net_state) {
        std::cout << "Network mode: ";
        switch (*net_state) {
            case NetworkMode::Online: std::cout << "Online\n"; break;
            case NetworkMode::Local: std::cout << "Local\n"; break;
            case NetworkMode::Offline: std::cout << "Offline\n"; break;
        }
    }
    
    return 0;
}
```

---

## Thread Safety

### Thread Safety Policies

FeatureManager supports three concurrency policies via template parameter:

```cpp
// 1. Single-threaded (default) - no synchronization overhead
FeatureManager<> manager;

// 2. Thread-safe with mutex - exclusive access
FeatureManager<MutexSynchronizationPolicy> ts_manager;

// 3. Shared mutex - concurrent reads, exclusive writes
FeatureManager<SharedMutexPolicy> concurrent_manager;
```

### Policy Details

**NoSynchronizationPolicy** (default):
- No locking
- Lowest overhead
- **Use:** Single-threaded applications

**MutexSynchronizationPolicy**:
- Uses `std::mutex`
- All operations exclusive
- **Use:** Multi-threaded with low contention

**SharedMutexPolicy**:
- Uses `std::shared_mutex` (C++20)
- Reads concurrent, writes exclusive
- **Use:** Many readers, few writers (e.g., query-heavy workloads)

### Factory Thread Safety

The global factory is **not** thread-safe by default. For multi-threaded registration:

**Option 1:** Change factory type globally in `FeatureManager.h`:
```cpp
using FeatureCheckFactory = ThreadSafeFactory<std::string, FeatureCheck>;
```

**Option 2:** Guard registration manually:
```cpp
std::mutex factory_mutex;

void register_callback(const std::string& key, auto creator) {
    std::lock_guard lock(factory_mutex);
    getFeatureCheckFactory().registerType(key, creator);
}
```

### Observer Thread Safety and Reentrancy

Observers are called **while holding the FeatureManager's internal lock**. This has two important implications:

**1. No Reentrancy:** Calling any FeatureManager method from within an observer will cause a **deadlock**:

```cpp
// ❌ This will DEADLOCK - observer tries to acquire lock that's already held
manager.addObserver([&manager](const std::string& name, bool state, bool success) {
    manager.enable("OtherFeature");  // DEADLOCK!
});

// ✅ Set a flag, process later
std::atomic<bool> needs_update{false};
std::string pending_feature;

manager.addObserver([&](const std::string& name, bool state, bool success) {
    if (name == "TriggerFeature" && state) {
        needs_update = true;
        pending_feature = "OtherFeature";
    }
});

// After enable() returns, lock is released - safe to call again
manager.enable("TriggerFeature");
if (needs_update) {
    manager.enable(pending_feature);  // Safe - outside observer
}
```

**Alternatives to Reentrancy:**
1. Use Implies/Requires relationships for automatic cascading
2. Queue operations for later processing
3. Use a separate thread/event loop
4. Redesign dependencies to avoid cascading enables

**2. Concurrent Policies Require Thread-Safe Observers:**

If using `MutexSynchronizationPolicy` or `SharedMutexPolicy`, multiple threads may trigger observers. Protect shared state accessed in observers:

```cpp
std::mutex log_mutex;

manager.addObserver([&log_mutex](const std::string& name, bool state, bool success) {
    std::lock_guard lock(log_mutex);
    log_file << name << " -> " << state << "\n";
});
```

---

## Performance Characteristics

| Operation | Complexity | Notes |
|-----------|------------|-------|
| `addFeature` | O(log n) | Map insertion with or without callback |
| `enable` | O(d × log n) | d = dependency depth (max 100) |
| `disable` | O(log n) | No dependency checking |
| `batchDisable` | O(n + k) | n = features (for validation), k = batch size |
| `isEnabled` | O(log n) | Simple map lookup |
| `validate` | O(n × d × log n) | Full graph validation |
| `batchEnable` | O(k × d × log n) | k = batch size, includes rollback tracking |
| `toJson` | O(n + r) | n = features, r = relationships |
| `fromJson` | O(n log n + r) | Plus O(n) factory lookups |
| `addRelationship` | O(log n) | Map insertion |
| `addObserver` | O(1) | Appends to vector |
| `removeObserver` | O(p) | p = number of observers (linear search) |
| `clearObservers` | O(p) | Clear both vectors |
| Factory `make` | O(log n) | Map lookup |

### Memory Usage

- **Per Feature:** ~200 bytes (node + relationships + strings)
- **Per Relationship:** ~48 bytes (set entry + string)
- **Per Observer:** ~144 bytes (function object + priority + ObserverId)
- **Per Batch Observer:** ~160 bytes (function object + priority + ObserverId)
- **Factory Entry:** ~80 bytes (key + creator function)

### Optimization Notes

1. **Hot Path:** `isEnabled()` is optimized for frequent checks
2. **Caching:** Relationship sets use `std::set` (sorted) for cache-friendly iteration
3. **Rollback:** Only tracks changed features, not full snapshots
4. **Depth Limit:** kMaxValidationDepth = 100 prevents stack overflow
5. **Lock-Free Paths:** NoSynchronizationPolicy eliminates all locking overhead

### Performance Characteristics

| Operation | Mechanism | Cost Driver |
|-----------|-----------|-------------|
| `isEnabled()` check | Single hash lookup + boolean read | O(1) — dominated by hash map access |
| Enable feature with dependencies | Dependency graph traversal + state updates | O(d) in dependency count — graph walk + per-dependency enable |
| Validate feature graph | Full graph traversal for cycle/conflict detection | O(V + E) in graph size |
| Serialize to JSON | Feature iteration + JSON string construction | O(n) in feature count — string allocation dominates |
| Deserialize from JSON | JSON parse + factory lookups per feature | O(n) in feature count — factory lookup per feature |

See `components/FeatureManager/results/` for current platform-specific benchmark data.

---

## Best Practices

### 1. Initialize Factory Early

**Always** register callbacks before loading configurations:

```cpp
// ✅ CORRECT ORDER
int main() {
    init_all_modules();      // Registers all callbacks
    load_feature_config();   // Loads from JSON - callbacks restored
}

// ❌ WRONG ORDER
int main() {
    load_feature_config();   // Callbacks not found!
    init_all_modules();      // Too late
}
```

### 2. Use Factory Keys for Persistence

For any feature that needs serialization, use factory-based registration:

```cpp
// ✅ Serializable
factory.registerType("gpu.check", /* ... */);
manager.addFeature("GPUFeature", "gpu.check");

// ❌ Not serializable
manager.addFeature("GPUFeature", []() { /* ... */ });
```

### 3. Organize Callback Keys Hierarchically

```cpp
// ✅ Clear organization
"graphics.renderer.opengl"
"graphics.renderer.vulkan"
"audio.output.wasapi"
"network.protocol.tcp"

// ❌ Unclear
"check1"
"gpu_validate"
```

### 4. Use RAII for Dynamic Features

For plugins or dynamically loaded modules:

```cpp
// ✅ Automatic cleanup
FeatureCheckRegistration reg("plugin.X", /* ... */);

// ❌ Manual cleanup required
factory.registerType("plugin.X", /* ... */);
// Must remember to unregister!
```

### 5. Handle Missing Callbacks Gracefully

When loading configurations, some callbacks might not be registered:

```cpp
auto result = FeatureManager<>::fromJson(json);
if (!result) {
    std::cerr << "Load warning: " << result.error() << "\n";
    // Some features might lack validation callbacks
    // Decide: fail completely or continue with warnings
}
```

### 6. Validate After Manual Changes

After using `disable()`, always validate:

```cpp
manager.disable("CoreFeature");

auto validation = manager.validate();
if (!validation) {
    std::cerr << "Graph inconsistent: " << validation.error() << "\n";
    // Decide: rollback or fix dependencies
}
```

### 7. Use Batch Operations for Atomicity

When enabling multiple related features:

```cpp
// ✅ Atomic
manager.batchEnable({"A", "B", "C"});

// ❌ Partial failure possible
manager.enable("A");
manager.enable("B");  // If this fails, "A" stays enabled
manager.enable("C");
```

### 8. Keep Validation Fast

Validation callbacks run frequently - keep them fast:

```cpp
// ✅ Fast check
factory.registerType("key", []() -> FeatureCheck {
    return []() {
        return has_flag() ? Expected<void>{} : unexpected("Missing flag");
    };
});

// ❌ Slow - blocks enable operations
factory.registerType("key", []() -> FeatureCheck {
    return []() {
        expensive_network_check();  // BAD!
        return {};
    };
});
```

### 9. Document Feature Dependencies

Use comments or external documentation:

```cpp
// GraphicsHigh requires:
// - GPU (hardware check)
// - BasicGraphics (base features)
// - DX12Support (API version)
manager.addFeature("GraphicsHigh");
manager.addRelationship("GraphicsHigh", Requires, "GPU");
manager.addRelationship("GraphicsHigh", Requires, "BasicGraphics");
manager.addRelationship("GraphicsHigh", Requires, "DX12Support");
```

### 10. Test Serialization Roundtrips

Always verify save/load preserves behavior:

```cpp
void test_roundtrip() {
    FeatureManager<> original;
    // Setup features...
    
    std::string json = original.toJson();
    auto restored = FeatureManager<>::fromJson(json);
    
    assert(restored.has_value());
    assert(original.isEnabled("A") == restored->isEnabled("A"));
    
    // Test validation still works
    auto result = restored->enable("ValidatedFeature");
    assert(result.has_value() || !result.error().empty());
}
```

---

## Troubleshooting

### Problem: Callbacks Not Restored After Deserialization

**Symptoms:** Features load correctly but validation doesn't run, or `addFeature` with key fails.

**Cause:** Callbacks not registered before loading JSON.

**Solution:**
```cpp
// Ensure factory initialization happens first
init_factory();           // Register all callbacks
auto manager = FeatureManager<>::fromJson(json);  // Then load
```

**Diagnostic:** Check error message - will say "Check key 'X' not found in factory".

---

### Problem: "Check key already registered" Error

**Symptoms:** `registerType()` returns false.

**Cause:** Key was registered twice (maybe by different modules).

**Solution:**
```cpp
// Check before registering
if (!factory.hasType("my.key")) {
    factory.registerType("my.key", /* ... */);
} else {
    std::cerr << "Key 'my.key' already registered\n";
}
```

**Alternative:** Use namespaced keys to avoid conflicts between modules.

---

### Problem: Feature Works Before Save But Not After Load

**Symptoms:** Direct callback works, but after save/load validation fails.

**Cause:** Used direct callback instead of factory key - callback not serialized.

**Solution:**
```cpp
// ❌ Direct callback - lost on save
manager.addFeature("Feature", []() { /* ... */ });

// ✅ Factory key - preserved
factory.registerType("feature.check", []() -> FeatureCheck {
    return []() { /* ... */ };
});
manager.addFeature("Feature", "feature.check");
```

---

### Problem: "Circular dependency detected"

**Symptoms:** `enable()` fails with cycle error message.

**Cause:** Dependency cycle in the graph (A → B → C → A).

**Solution:**
1. **Review error message** - shows exact cycle path
2. **Redesign relationships** - break the cycle
3. **Use Implies instead of Requires** if relationship is optional

**Example:**
```
Error: "Circular dependency detected: Graphics -> Renderer -> Context -> Graphics"

Fix: Graphics should not require Context; Context requires Graphics instead.
```

---

### Problem: Lifetime Issues with Member Functions

**Symptoms:** Crash or undefined behavior when factory callback executes.

**Cause:** Captured `this` pointer becomes invalid (object was destroyed).

**Solution:**
```cpp
// ❌ Dangling 'this'
factory.registerType("key", [this]() -> FeatureCheck {
    return [this]() { return this->check(); };  // 'this' may be invalid
});

// ✅ Use shared_ptr
auto self = std::make_shared<MyClass>();
factory.registerType("key", [self]() -> FeatureCheck {
    return [self]() { return self->check(); };  // 'self' kept alive
});
```

---

### Problem: Deadlock When Modifying Features in Observer

**Symptoms:** Application hangs/freezes when enabling or disabling features.

**Cause:** Observer callback called `enable()`, `disable()`, or another FeatureManager method, attempting to acquire a lock that's already held.

**Solution:**
```cpp
// ❌ Deadlock - lock already held during observer callback
manager.addObserver([&manager](auto...) {
    manager.enable("Other");  // DEADLOCK!
});

// ✅ Set flag, process after operation completes
std::atomic<bool> needs_enable{false};
manager.addObserver([&needs_enable](auto...) {
    needs_enable = true;
});

manager.enable("Trigger");  // Lock released after this returns

// Now safe to call again
if (needs_enable) {
    manager.enable("Other");  // OK - outside observer
}

// ✅ Better: Use Implies relationship for automatic cascading
manager.addRelationship("Trigger", FeatureRelationship::Implies, "Other");
manager.enable("Trigger");  // Both enabled atomically, no manual cascading
```

---

### Problem: Validation Callback Never Runs

**Symptoms:** Feature enables but validation logic doesn't execute.

**Cause:** Callback wasn't attached, or feature already enabled (validation only runs on state change).

**Solution:**
```cpp
// Verify callback is attached
if (!factory.hasType("my.check")) {
    std::cerr << "Callback 'my.check' not registered!\n";
}

// Force validation even if enabled
manager.disable("Feature");
auto result = manager.enable("Feature");  // Runs validation
```

---

### Problem: JSON Deserialization Fails

**Symptoms:** `fromJson()` returns error.

**Cause:** Invalid JSON syntax, missing callback keys, or incompatible format.

**Solution:**
```cpp
auto result = FeatureManager<>::fromJson(json);
if (!result) {
    std::cerr << "Parse error: " << result.error() << "\n";
    // Error indicates: syntax error, missing keys, or format issues
}

// Validate JSON externally first
// Ensure all callback keys are registered
```

---

### Problem: Performance Degradation with Many Features

**Symptoms:** `enable()` or `validate()` becomes slow with 1000+ features.

**Cause:** Deep dependency chains or expensive validation callbacks.

**Solution:**
1. **Profile:** Measure where time is spent
2. **Optimize Callbacks:** Ensure validations are fast (< 1ms)
3. **Limit Depth:** Keep dependency chains shallow (< 10 levels)
4. **Use Groups:** Aggregate features into groups to reduce graph size
5. **Consider Caching:** Cache validation results if checks are expensive

---

## Comparison with Other Libraries

| Library | Description & Use Case | FeatureManager Advantage |
|---------|------------------------|--------------------------|
| **gflags** | Google's command-line flag parser (e.g., `--feature=true`). Designed for static configuration at program launch - flags are simple boolean/string values without runtime relationships or dynamic state. Best for CLI tools and batch processing. | **Runtime Dependency Resolution:** FeatureManager handles complex interdependencies automatically (Requires/Implies/Conflicts). **Dynamic State:** Features can change at runtime with full validation. **No Server Needed:** Fully local, header-only library. **Serialization:** Complete graph persistence including validation logic. |
| **feature_flag** | Common pattern/basic implementations in C++ (e.g., simple toggle classes on GitHub, or compile-time `#ifdef` macros). Usually minimal or no support for relationships - developers manage conflicts/dependencies manually. Suited for small-scale projects or build-time flags. | **Automatic Conflict Detection:** FeatureManager prevents invalid combinations automatically. **Cycle Detection:** Reports circular dependencies with exact paths. **Transactional Semantics:** Batch operations with rollback. **Type-Safe Groups:** Custom enum states for domain logic. |
| **Unleash** | Open-source feature toggle *platform* with server infrastructure and client SDKs (including C++). Provides remote management, A/B testing, user segmentation, and gradual rollouts. Requires server setup, network calls, and dependency on HTTP libraries. Ideal for distributed teams managing features across many services. | **Zero Infrastructure:** FeatureManager is header-only with no servers or network. **Offline Use:** Works completely offline/embedded. **Local Validation:** Callbacks execute locally with full control. **Lower Latency:** No network round-trips for feature checks. **Embedded Systems:** Suitable for devices with no network. |

### When to Use Each

- **Use gflags:** Simple CLI flags for one-time configuration (e.g., logging levels, file paths)
- **Use feature_flag (pattern):** Small projects with 5-10 independent toggles, no dependencies
- **Use Unleash:** Distributed systems needing centralized control, A/B testing, remote management
- **Use FeatureManager:** Complex feature graphs with dependencies, local validation, embedded systems, games, desktop apps, or any scenario needing sophisticated dependency resolution without infrastructure overhead

---

## Conclusion

FeatureManager v3.0 provides a comprehensive solution for feature flag management with:

✅ **Automatic dependency resolution** with cycle detection  
✅ **Validation callbacks with full serialization** via factory system  
✅ **Type-safe group states** for domain-specific logic  
✅ **Transactional semantics** with automatic rollback  
✅ **Observer pattern** with reentry protection  
✅ **Module independence** via hierarchical callback registration  
✅ **High performance** with optimized graph algorithms  
✅ **Zero dependencies** - header-only library  

The callback factory system is a key innovation, solving the long-standing problem of serializing validation logic and making FeatureManager suitable for production systems that require persistence and reliable state restoration.

**Getting Started:**
1. Register callbacks with the factory using hierarchical keys
2. Add features using factory keys (not direct callbacks)
3. Define relationships between features
4. Enable features (dependencies auto-resolve)
5. Save/load complete graphs including validation logic

For questions, issues, or contributions, please contact the fat_p library maintainers.

---

**Document Version:** 3.0  
**Last Updated:** November 2025  
**FeatureManager Version:** 3.0
