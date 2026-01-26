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

### *From Hidden Dependencies to Explicit Wiring and Scoped Overrides*

*Fat-P Library — January 2026*

---

## Migration card

| Aspect | Detail |
|--------|--------|
| **From** | Global structs, singleton accessors, function tables, implicit globals in C modules |
| **To** | `fat_p::ServiceLocator` (typically `fat_p::DefaultServiceLocator` or `fat_p::ThreadSafeServiceLocator`) |
| **Problems solved** | Hidden dependencies, test isolation, configurable wiring at startup, controlled overrides |
| **Migration complexity** | Medium — you must identify dependencies and choose where to construct/register them |
| **Breaking changes** | Yes — call sites change from global access to locator-based access |

---

## The target pattern

In C, globals commonly look like this:

```c
// logger.h
extern struct Logger g_logger;

// db.h
extern struct Database g_db;
```

In C++, the goal is to create services at a **composition root** (startup) and pass a locator reference where needed:

```cpp
#include "fat_p/ServiceLocator.h"

using Services = fat_p::DefaultServiceLocator; // or ThreadSafeServiceLocator

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
    // If missing services should be treated as a programming/configuration error:
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

### 4) Migrate “test overrides” using scopes

Scopes are the core test seam. A scope holds a child locator that falls back to its parent.

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

ServiceLocator is useful when threading many individual dependencies through deep call stacks becomes unwieldy.
If you have a small number of dependencies or want constructor signatures to show them explicitly, plain constructor injection can be a better fit.

A pragmatic approach is:

- Use constructor injection inside leaf types.
- Use `ServiceLocator` at the edges (composition root + tests) to keep wiring manageable.

---

## Next references

- `Documentation/ServiceLocator/Overview - ServiceLocator.md`
- `Documentation/ServiceLocator/User Manual - ServiceLocator.md`
- `tests/test_ServiceLocator.cpp`
