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

## Executive summary

`fat_p::ServiceLocator` is a policy-based service registry designed for **explicit wiring at startup** and **fast resolution at runtime**, with **scoped overrides** for tests and temporary behavior changes.

It supports three registration styles:

- **Instance registration** (`registerInstance<T>(T&)`): non-owning; the caller keeps lifetime ownership.
- **Shared registration** (`registerShared<T>(std::shared_ptr<T>)`): shared ownership.
- **Factory registration** (`registerFactory<T>(factory, lifetime)`): singleton or transient creation.

For layering, a locator can create child scopes that fall back to a parent locator when a service is not found locally. This enables patterns like “production defaults in the root, test overrides in a scope”.

---

## Overview card

| Aspect | Details |
|--------|---------|
| **Component** | `fat_p::ServiceLocator` (and aliases: `DefaultServiceLocator`, `ThreadSafeServiceLocator`, `HotLoopServiceLocator`, `ThreadSafeHotLoopServiceLocator`) |
| **Problem solved** | Centralized registration + type-safe service lookup with scoped overrides |
| **When to use** | App/framework “composition roots”, integration tests, systems that need temporary overrides (feature flags, simulated backends) |
| **When not to use** | When you need compile-time dependency graphs, when services are frequently unregistered while pointers are in use, when type keys must be stable across DSOs/plugins without customization |
| **Key capability** | Parent/child layering via `makeScope()` and `makeChild()` |
| **Key trade-off** | Fast lookup and flexible overrides vs. weaker lifetime guarantees for raw-pointer resolves |

---

## Mental model

A `ServiceLocator` is a map from **(type, optional name)** to a service entry.

- If `name` is empty, the service is stored in the “unnamed” registry (fast path).
- If `name` is non-empty, the service is stored in the “named” registry keyed by `(typeId, name)`.

Resolution checks:

1. Current scope
2. Parent scope (if any)
3. Failure

A scope does not copy services. It only stores overrides and a pointer to its parent.

---

## Threading and policies

`ServiceLocator` does not hard-code synchronization. Concurrency is defined by its `ConcurrencyPolicy` template parameter.

Common aliases:

- `fat_p::DefaultServiceLocator` (single-threaded policy; no internal synchronization)
- `fat_p::ThreadSafeServiceLocator` (shared-mutex policy; supports concurrent resolves)
- `fat_p::HotLoopServiceLocator` (single-threaded + tiny thread-local MRU cache for unnamed resolves)
- `fat_p::ThreadSafeHotLoopServiceLocator` (shared-mutex + the same MRU cache)

Statistics are also policy-based. When using a concurrency policy that allows concurrent resolves, the statistics policy must use atomic increments (enforced by a `static_assert`).

---

## Error model

The API offers three “levels” of error handling:

- `tryResolve<T>(name)` → `T*` (returns `nullptr` if missing)
- `resolveExpected<T>(name)` → `Expected<std::reference_wrapper<T>, ServiceErrorInfo>`
- `resolve<T>(name)` → `T&` (enforces on failure; intended for “required” services)

For factory-registered services:

- `createExpected<T>(name)` creates a `std::shared_ptr<T>` (singleton or transient based on the registration)

For lifetime-carrying resolves:

- `resolveSharedExpected<T>(name)` returns a `std::shared_ptr<T>` when the registration is shared ownership or a singleton factory that has produced a shared instance.

---

## What scoped overrides look like

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

## Known limitations and design choices

- **Type keys**: the default type key is based on address identity of an internal token. This is fast and stable within a single binary, but not stable across DSO/plugin boundaries.
- **Raw-pointer resolves**: `tryResolve` and `resolveExpected` yield pointers/references. Callers must not continue using those pointers/references after the service is unregistered or overwritten.
- **Factories**: transient services must be created with `createExpected<T>()` (resolving them as references is rejected with `ServiceError::TransientRequiresCreate`).

---

## Read next

- **User Manual - ServiceLocator** (detailed API reference and patterns)
