---
doc_id: MG-SERVICELOCATOR-001
doc_type: "Migration Guide"
title: "Global Services to ServiceLocator"
from_pattern: "Global structs, singletons, function pointer tables"
to_component: "ServiceLocator"
fatp_version: "1.0"
cxx_standard: "C++20"
std_equivalent: null
std_since: null
boost_equivalent: null
migration_complexity: "Medium"
breaking_changes: true
last_verified: "2026-01-26"
---
# Migration Guide - Global Services to ServiceLocator

*Fat-P Library — January 2026*

---

**Scope:** This guide shows how to replace global service access (globals, singleton getters, function-pointer tables) with `fat_p::ServiceLocator`, including scoped overrides for tests.

**Not covered:**
- Designing stable type keys for plugin/DSO boundaries (custom `TypeKeyPolicy`)
- A full comparison of DI frameworks (beyond a brief alternatives section)
- Refactoring internal class design (interfaces vs concrete types) beyond the minimal steps shown here

**Prerequisites:**
- C++17+ (interfaces via virtual functions, `std::shared_ptr`, lambdas)
- Ability to identify “composition root” code (startup wiring)
- Basic unit testing familiarity (for scoped overrides)

---

## Migration Guide Card

**From:** Global structs, singleton accessors, function-pointer tables, implicit globals in C modules  
**To:** `fat_p::ServiceLocator` (typically `fat_p::DefaultServiceLocator` or `fat_p::ThreadSafeServiceLocator`)  
**Why migrate:** Make dependencies explicit, enable scoped overrides in tests, and control wiring at startup  
**Compatibility strategy:** Introduce a locator at the composition root; optionally keep a transitional `::global()` accessor while call sites are migrated  
**Mechanical steps:** 1) Identify global dependencies 2) Register services at startup 3) Pass `ServiceLocator&` to consumers 4) Replace global reads with `resolve/resolveExpected/tryResolve` 5) Use scopes for overrides  
**Behavioral equivalence:** Service behavior stays the same; only the access mechanism changes  
**Intentional differences:** Missing services can be treated as configuration errors (enforced) or as recoverable failures (`Expected`)  
**Failure model:** Globals: undefined behavior / null checks / ad-hoc error codes → `ServiceErrorInfo` via `Expected` or enforcement for required services  
**Threading model:** Choose `DefaultServiceLocator` (no internal synchronization) or `ThreadSafeServiceLocator` (shared-mutex protected registry) based on call patterns  
**Lifetime model:** Instance registrations are non-owning; shared registrations/factories return `shared_ptr`-managed lifetimes  
**Alternatives:** Constructor injection (manual), Boost.DI (framework), EnTT locator (global per type)  
**Verification:** Add/extend unit tests; use scopes to override dependencies; run the ServiceLocator test suite  
**Rollback plan:** Keep the old global accessor wrapper (or a `global()` locator) behind a feature flag until all call sites are migrated and validated

---

## The target pattern

In C, globals commonly look like this:

```c
// logger.h
extern struct Logger g_logger;

// db.h
extern struct Database g_db;
```

In C++, the target is to create services at a **composition root** (startup) and pass a locator reference where needed:

```cpp
#include "fat_p/ServiceLocator.h"

using Services = fat_p::DefaultServiceLocator; // or fat_p::ThreadSafeServiceLocator

void setup_services(Services& services);
void process_order(Services& services /*, ... */);
```

---

## Step-by-step migration

### 1) Define interfaces (optional but recommended)

If you previously used global structs directly, introduce minimal C++ interfaces so consumers depend on behavior, not concrete types.

```cpp
#include <string_view>

struct ILogger {
    virtual ~ILogger() = default;
    virtual void log(std::string_view msg) = 0;
};

struct IDatabase {
    virtual ~IDatabase() = default;
    virtual void exec(std::string_view sql) = 0;
};
```

### 2) Create a composition root and register services

Register services once during startup.

```cpp
#include "fat_p/ServiceLocator.h"
#include <memory>

struct FileLogger : ILogger {
    void log(std::string_view) override {}
};

struct SqliteDb : IDatabase {
    void exec(std::string_view) override {}
};

using Services = fat_p::DefaultServiceLocator;

void setup_services(Services& services)
{
    (void)services.registerShared<ILogger>(std::make_shared<FileLogger>());
    (void)services.registerShared<IDatabase>(std::make_shared<SqliteDb>());
}
```

If you still need a transitional “global registry”, use the per-instantiation global:

```cpp
// Transitional only: prefer passing Services& explicitly.
auto& services = fat_p::DefaultServiceLocator::global();
```

If you require concurrent resolves from multiple threads, prefer:

```cpp
auto& services = fat_p::ThreadSafeServiceLocator::global();
```

### 3) Replace global reads with `tryResolve` / `resolveExpected` / `resolve`

Update call sites to pull dependencies from the locator:

```cpp
void process_order(Services& services /*, const Order& order */)
{
    // Missing services treated as a configuration error:
    ILogger& logger = services.resolve<ILogger>();
    IDatabase& db   = services.resolve<IDatabase>();

    logger.log("Processing order");
    db.exec("INSERT INTO orders ...");
}
```

If you want explicit error handling (no enforcement), use `resolveExpected`:

```cpp
auto logger = services.resolveExpected<ILogger>();
if (!logger.has_value()) {
    // logger.error() is a ServiceErrorInfo
    return;
}
```

### 4) Migrate test overrides using scopes

A scope is a child locator that falls back to its parent.

```cpp
#include "fat_p/ServiceLocator.h"
#include <memory>

struct TestLogger : ILogger {
    int calls = 0;
    void log(std::string_view) override { ++calls; }
};

void test_example()
{
    Services root;
    (void)root.registerShared<ILogger>(std::make_shared<FileLogger>());
    (void)root.registerShared<IDatabase>(std::make_shared<SqliteDb>());

    auto scope = root.makeScope();

    auto testLogger = std::make_shared<TestLogger>();
    (void)scope.locator().registerShared<ILogger>(testLogger);

    process_order(scope.locator());

    // testLogger saw calls
}
```

### 5) Replace ad-hoc cleanup with RAII registration

If a test temporarily registers a service and wants it removed automatically, use `ServiceLocator::Registration`.

```cpp
void test_with_raii_registration()
{
    Services services;

    auto logger = std::make_shared<TestLogger>();

    auto reg = Services::Registration::registerSharedExpected<ILogger>(services, logger);
    if (!reg.has_value()) {
        return;
    }

    // ILogger is registered for this scope.

} // unregistered here
```

---

## Choosing between ServiceLocator and constructor injection

`ServiceLocator` is useful when threading many individual dependencies through deep call stacks becomes unwieldy.
If you have a small number of dependencies or want constructor signatures to show them explicitly, plain constructor injection can be a better fit.

A pragmatic approach is:

- Use constructor injection inside leaf types.
- Use `ServiceLocator` at the edges (composition root + tests) to keep wiring manageable.

---

## Next references

- `Documentation/ServiceLocator/Overview - ServiceLocator.md`
- `Documentation/ServiceLocator/User Manual - ServiceLocator.md`
- `tests/test_ServiceLocator.cpp`
