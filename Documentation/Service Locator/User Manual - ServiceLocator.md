---
doc_id: UM-SERVICELOCATOR-001
doc_type: "User Manual"
title: "ServiceLocator"
fatp_components: ["ServiceLocator"]
topics: ["registration", "resolution", "scopes", "factories", "RAII registration", "statistics", "hot loop caching"]
constraints: ["type keys not stable across DSOs by default", "transient factories require createExpected", "raw-pointer results require stable registration"]
cxx_standard: "C++20"
build_modes: ["Debug", "Release"]
last_verified: "2026-01-26"
audience: ["C++ developers", "test authors"]
status: "reviewed"
---
# User Manual - ServiceLocator

*Fat-P Library — January 2026*

---

**Scope:** This manual describes the public API of `fat_p::ServiceLocator`: registration, resolution, scopes, factories, RAII registration helpers, and statistics.

**Not covered:**
- Designing a `TypeKeyPolicy` for DSO/plugin boundaries
- Benchmark methodology and platform-specific performance tuning
- Writing custom `ConcurrencyPolicy` / `StatisticsPolicy` implementations

**Prerequisites:**
- C++17+ (templates, lambdas, `std::shared_ptr`)
- Familiarity with raw pointers vs `std::shared_ptr`
- Basic understanding of a composition root (startup wiring)

---

## User Manual Card

**Component:** `fat_p::ServiceLocator`  
**Primary use case:** Wire services at startup and resolve them by type (and optional name), with scoped overrides for tests  
**Integration pattern:** Construct a locator in the composition root, register instances/factories, pass `ServiceLocator&` where needed, and create scopes for temporary overrides  
**Key API:** `registerInstance`, `registerShared`, `registerFactory`, `tryResolve`, `resolveExpected`, `resolve`, `createExpected`, `makeScope`, `Registration`  
**std equivalent:** None  
**Migration from std:** N/A (pattern is not provided by the standard library)  
**Common mistakes:** Resolving a transient as `T&` instead of using `createExpected`, holding references across unregister/overwrite, assuming `global()` is one process-wide singleton for all aliases, mutating registrations concurrently with cache-enabled resolves  
**Performance notes:** Lookup is hash-based (average O(1)); unnamed services avoid name hashing; the optional MRU cache only applies to unnamed resolves; statistics overhead depends on the selected statistics policy

---

## Include

```cpp
#include "fat_p/ServiceLocator.h"
```

## Choosing a locator type

The component is a class template with a few convenience aliases.

| Alias | Concurrency | Resolve cache | Typical use |
|------:|-------------|---------------|-------------|
| `fat_p::DefaultServiceLocator` | none (single-threaded policy) | none | startup wiring + single-threaded programs/tests |
| `fat_p::ThreadSafeServiceLocator` | shared mutex | none | multiple threads calling `tryResolve/resolveExpected` |
| `fat_p::HotLoopServiceLocator` | none | MRU(2) for unnamed resolves | very hot unnamed resolves after registration stabilizes |
| `fat_p::ThreadSafeHotLoopServiceLocator` | shared mutex | MRU(2) for unnamed resolves | hot resolves + concurrent reads |

Notes:

- The MRU cache is **type-only** and applies only when `name` is empty.
- `ServiceLocator::global()` is **per instantiation**. If you need a process-wide global locator that supports
  concurrent resolves, prefer `fat_p::ThreadSafeServiceLocator::global()`.

---

## Basic registration

### Register a non-owning instance

```cpp
struct ILogger { virtual ~ILogger() = default; };
struct NullLogger : ILogger {};

fat_p::DefaultServiceLocator locator;

NullLogger logger;
auto r = locator.registerInstance<ILogger>(logger);

// r is Expected<void, ServiceErrorInfo>
if (!r.has_value()) {
    // handle error
}
```

### Register a shared instance

```cpp
auto sharedLogger = std::make_shared<NullLogger>();
(void)locator.registerShared<ILogger>(sharedLogger);
```

### Register named variants

Names allow multiple registrations of the same service type.

```cpp
NullLogger a;
NullLogger b;

(void)locator.registerInstance<ILogger>(a, "primary");
(void)locator.registerInstance<ILogger>(b, "secondary");

ILogger* p = locator.tryResolve<ILogger>("primary");
```

---

## Resolution

### `tryResolve<T>`

Returns a pointer or `nullptr`.

```cpp
ILogger* logger = locator.tryResolve<ILogger>();
if (logger == nullptr) {
    // missing
}
```

### `resolveExpected<T>`

Returns an `Expected<std::reference_wrapper<T>, ServiceErrorInfo>`.

```cpp
auto res = locator.resolveExpected<ILogger>();
if (!res.has_value()) {
    // res.error() contains a ServiceErrorInfo
} else {
    ILogger& logger = res.value().get();
}
```

### `resolve<T>`

Returns a reference and **enforces** if missing. Use this only when a missing service should terminate the program.

```cpp
ILogger& logger = locator.resolve<ILogger>();
```

---

## Factories

Factories are registered for `Singleton` or `Transient` lifetimes.

### Register a singleton factory

```cpp
struct Widget { int id; };

(void)locator.registerFactory<Widget>(
    []() -> std::unique_ptr<Widget> {
        return std::make_unique<Widget>(Widget{42});
    },
    fat_p::ServiceLifetime::Singleton
);

// Resolving as a reference will materialize the singleton (once) and cache it.
Widget& w = locator.resolve<Widget>();
```

Singleton creation is coordinated so that the factory runs once per registration, even with multiple threads resolving.

### Register a transient factory

```cpp
(void)locator.registerFactory<Widget>(
    []() -> std::shared_ptr<Widget> {
        return std::make_shared<Widget>(Widget{7});
    },
    fat_p::ServiceLifetime::Transient
);

// Transients must be created via createExpected().
auto created = locator.createExpected<Widget>();
if (created.has_value()) {
    std::shared_ptr<Widget> w = created.value();
}
```

If you attempt to `resolve<T>()` a transient factory registration, the call fails with
`ServiceError::TransientRequiresCreate`.

---

## Scoped overrides

### `makeScope()`

`makeScope()` returns an RAII object that owns a child locator.

```cpp
fat_p::DefaultServiceLocator root;

// root defaults
NullLogger prod;
(void)root.registerInstance<ILogger>(prod);

auto scope = root.makeScope();

// override in child
NullLogger override;
(void)scope.locator().registerInstance<ILogger>(override);

ILogger& inScope = scope.locator().resolve<ILogger>();
ILogger& inRoot  = root.resolve<ILogger>();
```

### `makeChild()`

`makeChild()` returns a child locator object directly (no RAII wrapper). Use this when you want explicit ownership.

---

## RAII registration

`ServiceLocator::Registration` is a small handle that unregisters a specific entry when it is destroyed.

```cpp
fat_p::DefaultServiceLocator locator;
NullLogger logger;

{
    auto reg = fat_p::DefaultServiceLocator::Registration::registerInstanceExpected<ILogger>(locator, logger);
    if (reg.has_value()) {
        // service is registered for this scope
    }
}
// service has been unregistered
```

Notes:

- The handle stores the service key (type + name) and calls `unregister()` in its destructor.
- Calling `reset()` unregisters the entry (if still registered) and makes the handle empty.

---

## Introspection and statistics

### `isRegistered<T>(name)`

```cpp
if (locator.isRegistered<ILogger>()) {
    // ...
}
```

This checks the current locator, then the parent chain.

### `stats()`

The concrete stats type depends on the statistics policy.

- `NoServiceLocatorStatisticsPolicy` returns no-op counters.
- `AtomicServiceLocatorStatisticsPolicy` provides atomic counters suitable for concurrent resolves.

---

## Lifetime guidance

- Prefer **`registerShared` + `resolveSharedExpected`** when you need the resolved value to carry lifetime.
- Prefer **`registerInstance`** for truly process-lifetime singletons (or objects owned by a larger subsystem).
- Avoid unregistering services while other code could still be using pointers/references returned by `tryResolve`/`resolveExpected`.

---

## Customizing type keys (DSO/plugin note)

The default type key is based on address identity of an internal token. If you need type keys stable across plugin
boundaries, provide a custom `TypeKeyPolicy` (for example, one based on `std::type_index` or a string id).

---

## Troubleshooting

- **`ServiceError::ServiceNotFound`**: no registration exists in the current locator or its parents.
- **`ServiceError::TransientRequiresCreate`**: the service is transient; use `createExpected<T>()`.
- **`ServiceError::FactoryReturnedNull` / `FactoryThrew`**: factory failed; the singleton remains uninitialized.

---

## Examples in the repository

- `tests/test_ServiceLocator.cpp`
- `benchmarks/benchmark_ServiceLocator.cpp`
