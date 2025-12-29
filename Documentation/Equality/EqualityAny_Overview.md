# EqualityAny: Type-Erased Comparison for std::any

*Updated December 2025*

> **Niche Component:** Most users should use `EqualityComparisons.h` directly. EqualityAny is for systems where types are erased at runtime — plugin architectures, heterogeneous data stores, serialization validation.

---

## Executive Summary

EqualityAny extends EqualityComparisons to handle `std::any` values through a **type registry** that recovers comparison capability after type erasure. When comparing two `std::any` objects, the registry dispatches to the appropriate typed comparator — including full epsilon propagation for registered container types. This enables tolerance-based comparison in systems where static typing is unavailable, without sacrificing the policy-based precision of the core library.

---

## The Problem

### Type Erasure Breaks Comparison

`std::any` deliberately erases type information. This breaks comparison:

```cpp
std::any a = 1.0;
std::any b = 1.0 + 1e-10;

// This doesn't compile — std::any has no operator==
// bool match = (a == b);

// This throws if types don't match exactly
// double va = std::any_cast<double>(a);

// This requires knowing the type at compile time
// ... but the whole point of std::any is that you don't
```

| Problem | Impact |
|---------|--------|
| No `operator==` on `std::any` | Can't compare without knowing types |
| `any_cast` requires exact type | Runtime type errors if wrong |
| Epsilon comparison impossible | No way to apply tolerance through type erasure |
| Nested any compounds the problem | `any(any(any(value)))` requires recursive unwrapping |

### When This Matters

Type-erased comparison is needed in:

- **Plugin systems**: Plugins return `std::any`; host must validate results
- **Heterogeneous stores**: `map<string, any>` configuration or state
- **Serialization validation**: Round-trip testing where types are recovered at runtime
- **Dynamic dispatch layers**: Type-erased interfaces between modules

---

## The Solution

```cpp
std::any a = std::vector<double>{1.0, 2.0, 3.0};
std::any b = std::vector<double>{1.0 + 1e-10, 2.0, 3.0};

bool match = areEqual(a, b, 1e-9);  // true: epsilon propagates into vector
```

EqualityAny maintains a **type registry** mapping `std::type_index` to comparison functions. When comparing two `std::any` objects:

1. Check if both are empty (equal) or one is empty (not equal)
2. Compare `type()` — different types are never equal
3. Look up the type in the registry
4. Dispatch to the registered comparator with epsilon propagation

### Pre-Registered Types

Common types are registered automatically for all four policies:

| Category | Types |
|----------|-------|
| Primitives | `int`, `double`, `float`, `bool`, `std::string` |
| Containers | `vector<int>`, `vector<double>`, `vector<float>`, `deque<double>` |
| Structural | `pair<double, double>`, `pair<int, double>`, `tuple<double, double, double>` |

Nested `std::any` (any containing any) is handled via special-case recursive unwrapping, not registry lookup.

**Note:** `long double` is not registered for `UlpComparisonPolicy` by design.

Custom types can be registered via `registerAnyType<T, Policy>()`.

---

## Architecture: Type Registry Dispatch

```cpp
template <typename Policy = StandardComparisonPolicy, typename... EpsParams>
bool areEqual(const std::any& a, const std::any& b, EpsParams... eps);
```

**The Registry Mechanism:**

The registry keys by **(type, policy)** pairs, with separate registries for default-epsilon and explicit-epsilon signatures:

```cpp
// Simplified: actual implementation uses Factory.h infrastructure
using RegistryKey = std::pair<std::type_index, std::type_index>;  // (type, policy)

std::map<RegistryKey, CompareFunctionDefault> defaultEpsRegistry;
std::map<RegistryKey, CompareFunctionExplicit> explicitEpsRegistry;
```

**Key design points:**

- Registry initialization is thread-safe (function-local static)
- Lookup is O(log N) via ordered map (N is small — typically tens of registered types)
- Epsilon parameters propagate through the registry into typed comparison
- Unregistered (type, policy) pairs return `false` (no crash, no throw)
- Additional types can be registered via `registerAnyType<T, Policy>()`; registration should be done during startup

---

## Feature Inventory

### 1. Epsilon Propagation Through Type Erasure

Tolerance flows from `areEqual(any, any, eps)` into the contained type:

```cpp
std::any a = std::vector<double>{1.0, 2.0, 3.0};
std::any b = std::vector<double>{1.0 + 1e-10, 2.0, 3.0};

areEqual(a, b, 1e-6);   // true: epsilon applied to each double
areEqual(a, b, 1e-12);  // false: difference exceeds tight tolerance
```

### 2. Nested std::any Support

Recursive unwrapping with depth limiting:

```cpp
std::any outer;
outer.emplace<std::any>(42);  // any containing any containing int

std::any outer2;
outer2.emplace<std::any>(42);

areEqual(outer, outer2);  // true: unwraps and compares inner values
```

**Depth limiting** prevents infinite recursion on pathological input:

```cpp
// 10+ levels of any-in-any nesting returns false (not throws)
constexpr size_t kMaxAnyRecursionDepth = 10;
```

### 3. Policy Selection

All four comparison policies work through type erasure when the contained (type, policy) pair is registered:

```cpp
std::any a = 1.0;
std::any b = 1.0 + 1e-15;

areEqual<StandardComparisonPolicy>(a, b);           // Default tolerance
areEqual<UlpComparisonPolicy>(a, b, 4.0);           // ULP comparison
areEqual<RelativeComparisonPolicy>(a, b, 1e-9);    // Relative tolerance
areEqual<HybridComparisonPolicy>(a, b, 1e-9, 1e-12); // Hybrid
```

### 4. Empty std::any Handling

Empty values compare correctly:

```cpp
std::any empty1, empty2;
std::any filled = 42;

areEqual(empty1, empty2);  // true: both empty
areEqual(empty1, filled);  // false: empty vs non-empty
```

### 5. Containers of std::any

`vector<any>` and similar containers work element-wise via `EqualityComparisons`:

```cpp
std::vector<std::any> v1 = {42, 3.14, std::string("hello")};
std::vector<std::any> v2 = {42, 3.14, std::string("hello")};

areEqual(v1, v2);  // true: each any element compared via registry
```

**Note:** Comparing `vector<any>` directly works because `EqualityComparisons` dispatches each element through the any comparator. If you store `vector<any>` *inside* a `std::any`, you must register `std::vector<std::any>` explicitly.

### 6. Unregistered Type Handling

Unknown types return `false` without throwing:

```cpp
struct CustomType { int x; };

std::any a = CustomType{42};
std::any b = CustomType{42};

areEqual(a, b);  // false: CustomType not registered (no crash)
```

Register custom types explicitly:

```cpp
registerAnyType<CustomType>();  // Now comparison works
```

---

## Thread Safety

- **Registry initialization**: Thread-safe (static initialization order)
- **Registry lookup**: Thread-safe (read-only after initialization)
- **Comparison operations**: Thread-safe (no shared mutable state)
- **Depth counter**: Thread-local (concurrent comparisons don't interfere)

---

## Why Not Alternatives?

| If You Need... | Why Not Manual | Fat-P Advantage |
|----------------|----------------|-----------------|
| Compare any values with tolerance | `any_cast` requires knowing exact type | Registry dispatches automatically |
| Support multiple policies | Write switch on type + policy | (type, policy) keyed registry |
| Handle nested any | Manual recursive unwrapping | Built-in with depth limiting |
| Thread-safe comparison | Manual synchronization | Function-local static initialization |

The alternative is writing a type switch for every comparison site — which defeats the purpose of type erasure.

---

## When to Use EqualityAny

**Use EqualityAny when:**
- Comparing `std::any` values with floating-point tolerance
- Building plugin systems that return type-erased results
- Validating heterogeneous configuration stores
- Testing serialization round-trips where types are recovered dynamically

**Use EqualityComparisons directly when:**
- Types are known at compile time (the common case)
- No type erasure in the data model
- Maximum performance required (registry lookup has small overhead)

---

## Integration Points

```
EqualityAny.h
    ├── uses
    │   ├── EqualityComparisons.h (typed comparison dispatch)
    │   ├── DiagnosticLogger_Core.h (mismatch logging)
    │   └── Factory.h (registry infrastructure)
    │
    └── used by
        ├── Plugin validation layers
        ├── Heterogeneous data stores
        └── Serialization test harnesses
```

---

## Limitations

| Limitation | Rationale |
|------------|-----------|
| Unregistered (type, policy) pairs return `false` | No universal comparison without type knowledge |
| Registration should be done at startup | Thread-safe but not designed for dynamic registration |
| Depth limit on nested any | Prevents stack overflow on malicious input |
| Registry lookup overhead | Small cost vs direct typed comparison |

---

## Final Assessment

EqualityAny solves a narrow but real problem: **comparing type-erased values with floating-point tolerance**. It's not needed for most use cases — if you know your types at compile time, use `EqualityComparisons.h` directly.

But when types are erased — plugins, heterogeneous stores, dynamic dispatch — EqualityAny recovers full comparison capability including epsilon propagation, policy selection, and nested container support.

**Type erasure no longer means giving up tolerance-based comparison.**

---

*EqualityAny.h — Fat-P Library*
*Companion to EqualityComparisons.h — see EqualityComparisons_Overview.md for the core library*
