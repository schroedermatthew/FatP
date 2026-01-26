---
doc_id: OV-SERVICELOCATOR-001
doc_type: "Overview"
title: "ServiceLocator"
fatp_components: ["ServiceLocator"]
topics: ["dependency injection", "service registry", "scoped overrides", "factories", "test seams"]
constraints: ["no ownership for instance registrations", "raw-pointer resolves", "type key stability across DSOs"]
cxx_standard: "C++20"
std_equivalent: null
boost_equivalent: null
build_modes: ["Debug", "Release"]
last_verified: "2026-01-26"
audience: ["C++ developers", "library maintainers", "test authors"]
status: "reviewed"
---
# Overview - ServiceLocator

*Fat-P Library — January 2026*

---

**Scope:** This document explains what `fat_p::ServiceLocator` is, the problems it targets, and the core mental model (registration, resolution, scopes, and factories).

**Not covered:**
- Full API reference and edge cases (see **User Manual - ServiceLocator**)
- Benchmark methodology and detailed performance tuning
- Designing a `TypeKeyPolicy` for DSO/plugin boundaries (see **Foundations - ABI Stability and Module Boundaries**)

**Prerequisites:**
- C++17+ fundamentals (templates, lambdas, `std::shared_ptr`)
- Basic ownership concepts (raw pointer vs `shared_ptr`)
- Familiarity with a “composition root” (where the program wires dependencies) is helpful

---

## Overview Card

**Component:** `fat_p::ServiceLocator` (aliases: `DefaultServiceLocator`, `ThreadSafeServiceLocator`, `HotLoopServiceLocator`, `ThreadSafeHotLoopServiceLocator`)  
**Problem solved:** Centralized runtime wiring plus type-keyed lookup with scoped overrides  
**When to use:** Composition roots, integration tests, systems that need temporary overrides (feature flags, simulated backends)  
**When NOT to use:** When you require compile-time dependency graphs, when services are frequently unregistered while pointers are still in use, when type keys must be stable across DSOs/plugins without customization  
**Key guarantee:** Resolution searches the current scope first, then the parent chain; singleton factories run once per registration (with cycle detection)  
**std equivalent:** None  
**Boost equivalent:** None  
**Other alternatives:** Constructor injection (manual), Boost.DI (if a framework is acceptable), EnTT locator (global per type, if available)  
**Read next:** **User Manual - ServiceLocator**, `tests/test_ServiceLocator.cpp`, **Foundations - ABI Stability and Module Boundaries**

---

## Mental model

A `ServiceLocator` is a registry keyed by **(type, optional name)**:

- **Unnamed** services use an internal registry keyed by type only.
- **Named** services use a registry keyed by `(type, name)`.

Resolution checks:

1. Current locator
2. Parent locator (if any)
3. Failure

A scope does not copy services. It stores overrides plus a pointer to its parent.

---

## Registration styles

`ServiceLocator` supports three registration styles:

- **Instance registration**: `registerInstance<T>(T&, name)` stores a non-owning pointer. The caller owns lifetime.
- **Shared registration**: `registerShared<T>(std::shared_ptr<T>, name)` stores shared ownership.
- **Factory registration**: `registerFactory<T>(factory, lifetime, name)` stores a factory plus a lifetime (`Singleton` or `Transient`).

---

## Resolution styles

The API exposes three “levels” of failure handling:

- `tryResolve<T>(name)` → `T*` (returns `nullptr` if missing)
- `resolveExpected<T>(name)` → `Expected<std::reference_wrapper<T>, ServiceErrorInfo>`
- `resolve<T>(name)` → `T&` (enforces on failure; intended for required services)

Factory-registered services are created via:

- `createExpected<T>(name)` → `Expected<std::shared_ptr<T>, ServiceErrorInfo>`

Lifetime-carrying resolves are available via:

- `resolveSharedExpected<T>(name)` → `Expected<std::shared_ptr<T>, ServiceErrorInfo>`

---

## Scoped overrides

`makeScope()` creates a child locator that falls back to the parent.

```cpp
#include "fat_p/ServiceLocator.h"

struct ILogger { virtual ~ILogger() = default; };
struct NullLogger : ILogger {};
struct TestLogger : ILogger {};

void example()
{
    fat_p::DefaultServiceLocator root;
    NullLogger prodLogger;
    (void)root.registerInstance<ILogger>(prodLogger);

    auto scope = root.makeScope();
    TestLogger testLogger;
    (void)scope.locator().registerInstance<ILogger>(testLogger);

    // In the scope, ILogger resolves to TestLogger.
    ILogger& inScope = scope.locator().resolve<ILogger>();

    // In the root, ILogger resolves to NullLogger.
    ILogger& inRoot = root.resolve<ILogger>();

    (void)inScope;
    (void)inRoot;
}
```

---

## Policies and concurrency

`ServiceLocator` does not hard-code synchronization. Concurrency is defined by its `ConcurrencyPolicy` template parameter.

Common aliases:

- `fat_p::DefaultServiceLocator` (no internal synchronization)
- `fat_p::ThreadSafeServiceLocator` (shared-mutex protected registry)
- `fat_p::HotLoopServiceLocator` (optional thread-local MRU cache for unnamed resolves)
- `fat_p::ThreadSafeHotLoopServiceLocator` (shared-mutex protected registry + the MRU cache)

Notes:

- The MRU cache is **type-only** and applies only when `name` is empty.
- Cache hits do not acquire the shared mutex. Treat registration/unregistration/clear as a startup/shutdown operation or quiesce threads first.

Statistics are policy-based as well. When using a concurrency policy that allows concurrent resolves, the statistics policy must provide atomic increments (enforced by `static_assert`).

---

## Known limitations and design choices

- **Type keys:** the default type key uses address-identity of an internal token. This is stable within a single binary, but not stable across DSO/plugin boundaries.
- **Pointer/reference resolves:** `tryResolve` and `resolveExpected` yield pointers/references. Callers must not keep those across unregister/overwrite operations.
- **Transients:** transient factories must be created with `createExpected<T>()`; resolving them as references is rejected with `ServiceError::TransientRequiresCreate`.

---

## Read next

- **User Manual - ServiceLocator** (API surface, patterns, and gotchas)
