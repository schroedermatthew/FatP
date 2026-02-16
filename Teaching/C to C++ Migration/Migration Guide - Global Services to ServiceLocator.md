---
doc_id: MG-SERVICELOCATOR-001
doc_type: "Migration Guide"
title: "Global Services to ServiceLocator"
fatp_components: ["ServiceLocator"]
topics: ["dependency injection", "service locator pattern", "global state elimination", "testability", "scoped overrides", "composition root"]
constraints: ["global state in legacy code", "test isolation", "startup wiring complexity", "thread-safety requirements"]
cxx_standard: "C++20"
std_equivalent: null
std_since: null
boost_equivalent: null
from_language: "C/C++"
to_language: "C++"
to_standard: "C++20"
from_pattern: ["global structs", "singleton accessors", "function pointer tables", "extern declarations"]
to_component: ["ServiceLocator"]
compatibility: ["incremental migration", "transitional global accessor"]
rollout: ["dual-stack", "feature flag", "per-module migration"]
migration_complexity: "Medium"
breaking_changes: true
last_verified: "2026-01-26"
audience: ["C++ developers", "C developers", "library maintainers", "AI assistants"]
status: "reviewed"
---

# Migration Guide - Global Services to ServiceLocator

*Fat-P Library — January 2026*

---

## Scope

This guide shows how to replace global service access patterns with `fat_p::ServiceLocator`, enabling explicit dependency wiring and scoped overrides for testing.

**Covered patterns:**

- C-style `extern` globals (`extern struct Logger g_logger;`)
- C++ singleton accessors (`Logger::instance()`)
- Function pointer tables for runtime polymorphism
- Ad-hoc service registries using `std::map<string, void*>`

**Target outcome:** Services are registered at a composition root (startup) and resolved explicitly, with test-time overrides via scoped child locators.

---

## Not covered

- **Custom TypeKeyPolicy for DSO/plugin boundaries.** The default type-key policy uses address identity, which is not stable across shared library boundaries. If you load services from plugins, you need a custom policy (see User Manual).
- **Full DI framework comparison.** We compare alternatives briefly but do not provide a comprehensive DI framework evaluation.
- **Interface extraction refactoring.** We show minimal interface definitions but do not cover broader refactoring strategies for legacy code.
- **Performance optimization.** We note performance characteristics but do not cover advanced caching strategies.

---

## Prerequisites

- **C++17 or later.** ServiceLocator requires `std::string_view`, `std::shared_ptr`, and lambda support.
- **Ability to identify composition root code.** The "composition root" is the single location where you wire dependencies together—typically `main()` or application startup. If your codebase has no clear composition root, you will need to create one.
- **Basic unit testing familiarity.** Scoped overrides are primarily useful for testing; understanding test fixtures helps.
- **Read the User Manual first.** This guide assumes familiarity with `registerInstance`, `registerShared`, `tryResolve`, `resolveExpected`, and `makeScope`.

---

## Migration Guide Card

| Attribute | Value |
|-----------|-------|
| **From** | Global structs, singleton accessors, function-pointer tables, `extern` declarations |
| **To** | `fat_p::ServiceLocator` (typically `DefaultServiceLocator` or `ThreadSafeServiceLocator`) |
| **Why migrate** | Make dependencies explicit; enable scoped overrides in tests; control wiring at startup; eliminate hidden coupling |
| **Compatibility strategy** | Introduce a locator at the composition root; optionally keep a transitional `::global()` accessor while call sites migrate |
| **Behavioral equivalence** | Service behavior stays the same; only the access mechanism changes |
| **Intentional differences** | Missing services are explicit errors (`ServiceErrorInfo`) rather than null pointers or undefined behavior |
| **Failure model** | Globals: undefined behavior, null checks, ad-hoc error codes → `Expected<T, ServiceErrorInfo>` or enforcement for required services |
| **Threading model** | Choose `DefaultServiceLocator` (no synchronization) or `ThreadSafeServiceLocator` (shared-mutex protected) based on access patterns |
| **Lifetime model** | Instance registrations are non-owning; shared registrations and factories return `shared_ptr`-managed lifetimes |
| **Verification** | Add/extend unit tests; use scopes to override dependencies; run sanitizers; verify no use-after-free |
| **Rollback plan** | Keep the old global accessor (or `global()` locator) behind a feature flag until all call sites are migrated and validated |

---

## Alternatives

Before committing to ServiceLocator, consider whether a simpler or different approach fits your needs.

| Alternative | Description | When to prefer | When to avoid |
|-------------|-------------|----------------|---------------|
| **Constructor injection** | Pass dependencies as constructor parameters | Small dependency graphs; leaf classes; want explicit signatures | Deep call stacks; many dependencies; wiring becomes verbose |
| **Boost.DI** | Automatic dependency injection framework | Want automatic wiring; willing to accept framework dependency | Need minimal dependencies; want explicit control |
| **EnTT locator** | One global per type, no scopes | Simple cases; single global per service is acceptable | Need scoped overrides; need named services; need parent/child layering |
| **Keep globals** | Don't migrate | Truly trivial programs; no testing requirements | Any production code; any code requiring tests |

**Recommendation:** Use constructor injection for leaf types (classes that don't resolve further dependencies). Use ServiceLocator at the edges—composition root and test fixtures—to keep wiring manageable. This hybrid approach gives explicit signatures where they matter and centralized wiring where it reduces boilerplate.

---

## Mapping: From → To

### Pattern mapping

| Legacy pattern | ServiceLocator equivalent | Notes |
|----------------|---------------------------|-------|
| `extern struct Logger g_logger;` | `locator.registerInstance<ILogger>(logger)` | Non-owning; caller manages lifetime |
| `Logger::instance()` | `locator.resolve<ILogger>()` | Returns reference; enforces if missing |
| `Logger* getLogger()` returning nullptr on missing | `locator.tryResolve<ILogger>()` | Returns pointer; nullptr if missing |
| `getLoggerOrError()` returning error code | `locator.resolveExpected<ILogger>()` | Returns `Expected<ref, ServiceErrorInfo>` |
| `std::shared_ptr<Logger> getSharedLogger()` | `locator.tryResolveShared<ILogger>()` | Returns `shared_ptr`; empty if unavailable |
| Function pointer table | `locator.registerInstance<IService>(impl)` | Replace vtable-style dispatch with interface |
| `std::map<string, Service*>` | `locator.registerInstance<IService>(svc, "name")` | Named services |
| Test double injection via preprocessor | `scope.locator().registerInstance<IService>(mock)` | Scoped override |

### API mapping

| Operation | Legacy | ServiceLocator |
|-----------|--------|----------------|
| Get service (must exist) | `g_logger` or `Logger::instance()` | `locator.resolve<ILogger>()` |
| Get service (may be missing) | `getLogger()` with null check | `locator.tryResolve<ILogger>()` |
| Get service with error info | Custom error handling | `locator.resolveExpected<ILogger>()` |
| Get shared ownership | `getSharedLogger()` | `locator.tryResolveShared<ILogger>()` or `resolveSharedExpected<ILogger>()` |
| Register at startup | Assignment to global | `locator.registerInstance<T>(instance)` or `registerShared<T>(ptr)` |
| Override for test | Preprocessor or link-time | `auto scope = locator.makeScope(); scope.locator().registerInstance<T>(mock);` |
| Create new instance each call | Factory function | `locator.registerFactory<T>(factory, ServiceLifetime::Transient)` then `createExpected<T>()` |
| Lazy singleton | `static` local or `call_once` | `locator.registerFactory<T>(factory, ServiceLifetime::Singleton)` then `resolve<T>()` |

### Locator type selection

| Requirement | Locator type |
|-------------|--------------|
| Single-threaded, no caching | `fat_p::DefaultServiceLocator` |
| Multi-threaded concurrent resolves | `fat_p::ThreadSafeServiceLocator` |
| Single-threaded, hot-path resolves | `fat_p::HotLoopServiceLocator` |
| Multi-threaded, hot-path resolves | `fat_p::ThreadSafeHotLoopServiceLocator` |

---

## Step-by-step migration plan

### Phase 0: Understand the cost model

Before migrating, understand ServiceLocator's performance characteristics:

| Resolution type | Typical latency | Notes |
|-----------------|-----------------|-------|
| Unnamed `tryResolve<T>()` | ~3 ns | Type pointer lookup only |
| Unnamed with MRU cache hit | ~1.5 ns | `HotLoopServiceLocator` variants |
| Named `tryResolve<T>("name")` | ~20-25 ns | String hashing overhead |

**Guidance:** Use unnamed services for hot-path resolution. Use named services for configuration-time lookups or when you need multiple instances of the same interface type.

### Phase 1: Define interfaces

If you currently access concrete types directly, introduce minimal interfaces so consumers depend on behavior, not implementation.

```cpp
// Before: consumers use FileLogger directly
struct FileLogger {
    void log(std::string_view msg);
};
extern FileLogger g_logger;

// After: consumers depend on ILogger interface
struct ILogger {
    virtual ~ILogger() = default;
    virtual void log(std::string_view msg) = 0;
};

struct FileLogger : ILogger {
    void log(std::string_view msg) override { /* ... */ }
};
```

**Why interfaces matter:** Without an interface, you cannot substitute a test double. The locator registers `ILogger`; tests can register `MockLogger` implementing the same interface.

### Phase 2: Create a composition root

Identify or create the single location where services are wired together.

```cpp
#include "fat_p/ServiceLocator.h"

// Choose your locator type based on threading requirements
using Services = fat_p::DefaultServiceLocator;

// The composition root: called once at startup
void setup_services(Services& services)
{
    // Shared registration: locator shares ownership
    auto logger = std::make_shared<FileLogger>();
    auto loggerReg = services.registerShared<ILogger>(logger);
    if (!loggerReg.has_value()) {
        // Handle registration failure (rare at startup)
        std::terminate();
    }

    auto db = std::make_shared<SqliteDatabase>();
    auto dbReg = services.registerShared<IDatabase>(db);
    if (!dbReg.has_value()) {
        std::terminate();
    }
}

int main()
{
    Services services;
    setup_services(services);

    // Pass services to application code
    run_application(services);
}
```

### Phase 3: Replace global reads incrementally

Update call sites one module at a time. During transition, you can use a global locator as a bridge.

```cpp
// Transitional: global locator accessor (remove after full migration)
Services& global_services()
{
    // Or use: return fat_p::ThreadSafeServiceLocator::global();
    static Services instance;
    return instance;
}

// Migrate call sites from:
void process_order_old(const Order& order)
{
    g_logger.log("Processing order");  // Old: global access
    g_db.exec("INSERT INTO orders...");
}

// To:
void process_order(Services& services, const Order& order)
{
    ILogger& logger = services.resolve<ILogger>();  // New: explicit dependency
    IDatabase& db = services.resolve<IDatabase>();

    logger.log("Processing order");
    db.exec("INSERT INTO orders...");
}

// Or, during transition:
void process_order_transitional(const Order& order)
{
    auto& services = global_services();
    ILogger& logger = services.resolve<ILogger>();
    // ...
}
```

### Phase 4: Add test overrides using scopes

A scope is a child locator that shadows parent registrations.

```cpp
void test_order_processing_logs_correctly()
{
    // Production services
    Services root;
    setup_services(root);

    // Create a scope for test overrides
    auto scope = root.makeScope();

    // Override logger with test double
    auto mockLogger = std::make_shared<MockLogger>();
    auto reg = scope.locator().registerShared<ILogger>(mockLogger);
    ASSERT_TRUE(reg.has_value());

    // Test using scoped locator
    Order testOrder{/* ... */};
    process_order(scope.locator(), testOrder);

    // Verify mock was called
    ASSERT_EQ(mockLogger->messages.size(), 1);
    ASSERT_TRUE(mockLogger->messages[0].contains("Processing order"));

}  // scope destroyed; override removed automatically
```

**Alternative: `makeChild()` for non-RAII usage:**

```cpp
// makeChild() returns a ServiceLocator directly (no RAII wrapper)
auto child = root.makeChild();
child.registerShared<ILogger>(mockLogger);
// Child must be kept alive manually; no automatic cleanup
```

### Phase 5: Use RAII registration for temporary overrides

For fine-grained control, use `Registration` helpers that unregister on destruction.

```cpp
void test_with_raii_registration()
{
    Services services;
    setup_services(services);

    auto mockLogger = std::make_shared<MockLogger>();

    // Registration unregisters automatically when reg goes out of scope
    auto reg = Services::Registration::registerSharedExpected<ILogger>(
        services, mockLogger);

    if (!reg.has_value()) {
        FAIL() << "Registration failed: " << reg.error();
        return;
    }

    // ILogger now resolves to mockLogger
    run_test_code(services);

}  // mockLogger unregistered here
```

### Phase 6: Remove transitional global accessors

Once all call sites pass `Services&` explicitly, remove the global accessor:

```cpp
// DELETE THIS after migration:
// Services& global_services() { ... }
```

---

## Compatibility and ABI boundaries

### No C ABI exposure

ServiceLocator is a C++ template; it does not expose a C ABI. If you need to pass services across a C boundary, resolve them to raw pointers or wrap in C-compatible handles.

```cpp
// C-compatible wrapper (if needed)
extern "C" {
    void* get_logger_handle(void* services_ptr)
    {
        auto* services = static_cast<Services*>(services_ptr);
        return services->tryResolve<ILogger>();
    }
}
```

### DSO/plugin boundaries

**Warning:** The default `TypeKeyPolicy` uses `&typeid(T)` address identity. This is **not stable across shared library boundaries**. If you resolve a type registered in one DSO from another DSO, the type keys may differ.

**Workarounds:**

1. Register and resolve services only from the main executable.
2. Use a custom `TypeKeyPolicy` with stable string-based keys.
3. Use named services with explicit string identifiers.

### Incremental migration compatibility

During migration, old and new code can coexist:

- Old code uses globals → Still works (globals still exist)
- New code uses locator → Works (locator is populated at startup)
- Globals and locator can reference the same instances

```cpp
// Bridge: register the existing global into the locator
FileLogger g_logger;  // Legacy global

void setup_services(Services& services)
{
    // Register the global instance (non-owning)
    services.registerInstance<ILogger>(g_logger);
}
```

---

## Lifetime and ownership model

### Ownership rules

| Registration method | Ownership | Lifetime responsibility |
|--------------------|-----------|------------------------|
| `registerInstance<T>(ref)` | Non-owning | Caller must keep instance alive while registered |
| `registerShared<T>(shared_ptr)` | Shared | Locator holds a `shared_ptr`; instance lives until last reference released |
| `registerFactory<T>(..., Singleton)` | Locator-owned | Locator holds created `shared_ptr` |
| `registerFactory<T>(..., Transient)` | Caller-owned | Each `createExpected()` returns a new `shared_ptr` |

### Teardown ordering

Services are unregistered in reverse order of registration when:

1. `clear()` is called
2. The locator is destroyed
3. A `Scope` is destroyed (for scoped registrations only)

**Critical:** If service A depends on service B, register B before A. On teardown, A is unregistered before B, preventing use-after-free.

```cpp
void setup_services(Services& services)
{
    // Register in dependency order: B before A
    services.registerShared<IDatabase>(db);    // B: no dependencies
    services.registerShared<IRepository>(repo); // A: depends on IDatabase
}
// On destruction: IRepository unregistered first, then IDatabase
```

### Pointer/reference validity

**Fact:** Pointers and references returned by `tryResolve` and `resolve` become invalid after the corresponding service is unregistered or overwritten.

**Guidance:**

- Do not cache pointers across potential registration changes.
- Use `resolveSharedExpected()` or `tryResolveShared()` if you need to extend lifetime.
- Treat registration as a startup/shutdown operation; do not mutate the registry during normal operation.

---

## Thread-safety and reentrancy

### Guarantees by locator type

| Locator type | Concurrent resolve | Concurrent register | Notes |
|--------------|-------------------|---------------------|-------|
| `DefaultServiceLocator` | ❌ Not safe | ❌ Not safe | Caller must synchronize all access |
| `ThreadSafeServiceLocator` | ✅ Safe | ❌ Exclusive | Register/unregister/clear take exclusive lock |
| `HotLoopServiceLocator` | ❌ Not safe | ❌ Not safe | Thread-local cache; single-threaded only |
| `ThreadSafeHotLoopServiceLocator` | ✅ Safe (cache hits lockless) | ❌ Exclusive | Cache hits bypass mutex |

### Non-guarantees

- **Do not register/unregister concurrently with resolve on cache-enabled locators.** Cache hits do not take locks; concurrent mutation can cause stale reads.
- **Singleton factory execution is serialized.** Only one singleton factory executes at a time across the entire locator family (parent + children). This prevents deadlocks but can serialize startup.
- **Circular dependencies are detected per call chain only.** Cross-thread circular waits may still deadlock; the locator serializes factory execution to prevent this.

### Reentrancy

**Fact:** Singleton factories may safely resolve other services from the same locator. If a factory attempts to resolve its own service (directly or transitively), the locator detects the cycle and returns `ServiceError::CircularDependency`.

```cpp
// Safe: factory resolves a different service
services.registerFactory<IRepository>(
    [&services]() {
        auto& db = services.resolve<IDatabase>();  // OK: different service
        return std::make_unique<SqlRepository>(db);
    },
    ServiceLifetime::Singleton);

// Detected: circular dependency
services.registerFactory<IServiceA>(
    [&services]() {
        services.resolve<IServiceA>();  // ERROR: circular
        return std::make_unique<ServiceAImpl>();
    },
    ServiceLifetime::Singleton);
```

---

## Error and failure model

### Legacy error patterns → ServiceLocator equivalents

| Legacy pattern | ServiceLocator equivalent |
|----------------|---------------------------|
| Return `nullptr` | `tryResolve<T>()` returns `nullptr` |
| Return error code | `resolveExpected<T>()` returns `Expected<ref, ServiceErrorInfo>` |
| Assert/abort | `resolve<T>()` enforces (terminates in debug, UB in release without `FATP_ALWAYS_ENFORCE`) |
| Throw exception | `resolve<T>()` with enforcement, or check `Expected` and throw |

### ServiceErrorInfo codes

| Error code | Meaning | Common cause |
|------------|---------|--------------|
| `ServiceNotFound` | No registration for this type/name | Missing `registerXxx` call |
| `CircularDependency` | Factory tried to resolve itself | Dependency cycle in factories |
| `FactoryReturnedNull` | Factory returned nullptr | Bug in factory implementation |
| `FactoryThrew` | Factory threw an exception | Bug in factory implementation |
| `WrongLifetime` | Operation not valid for this lifetime | Calling `createExpected` on non-transient |
| `RegistrationExists` | Service already registered (overwrite prevented) | Duplicate registration without `AllowOverwrite` |

### Choosing error handling strategy

```cpp
// Strategy 1: Required service (configuration error if missing)
ILogger& logger = services.resolve<ILogger>();  // Enforces; terminates if missing

// Strategy 2: Optional service (graceful degradation)
IAnalytics* analytics = services.tryResolve<IAnalytics>();
if (analytics) {
    analytics->track(event);
}
// else: no analytics configured; skip silently

// Strategy 3: Explicit error handling
auto result = services.resolveExpected<ILogger>();
if (!result.has_value()) {
    std::cerr << "Logger not configured: " << result.error().message << "\n";
    return ErrorCode::MissingDependency;
}
ILogger& logger = result.value().get();
```

---

## Verification plan

### Unit tests

1. **Verify registration succeeds:**
   ```cpp
   auto result = services.registerShared<ILogger>(logger);
   ASSERT_TRUE(result.has_value()) << result.error();
   ```

2. **Verify resolution returns expected instance:**
   ```cpp
   auto* resolved = services.tryResolve<ILogger>();
   ASSERT_EQ(resolved, logger.get());
   ```

3. **Verify scoped overrides work:**
   ```cpp
   auto scope = services.makeScope();
   scope.locator().registerShared<ILogger>(mockLogger);
   ASSERT_EQ(scope.locator().tryResolve<ILogger>(), mockLogger.get());
   ASSERT_EQ(services.tryResolve<ILogger>(), realLogger.get());  // Parent unchanged
   ```

4. **Verify missing service behavior:**
   ```cpp
   ASSERT_EQ(services.tryResolve<IUnregistered>(), nullptr);
   auto result = services.resolveExpected<IUnregistered>();
   ASSERT_FALSE(result.has_value());
   ASSERT_EQ(result.error().error, ServiceError::ServiceNotFound);
   ```

### Sanitizer runs

Run the full test suite under:

- **AddressSanitizer (ASan):** Detect use-after-free from stale pointers
- **ThreadSanitizer (TSan):** Detect data races in concurrent access
- **UndefinedBehaviorSanitizer (UBSan):** Detect undefined behavior

```bash
# Example with CMake
cmake -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined" ..
make && ctest

cmake -DCMAKE_CXX_FLAGS="-fsanitize=thread" ..
make && ctest
```

### Integration tests

1. **Startup test:** Application starts successfully with locator-based wiring.
2. **Shutdown test:** Application shuts down cleanly; no leaks (run under ASan).
3. **Feature parity test:** Behavior matches pre-migration behavior for key workflows.

### Benchmark sanity check

After migration, verify no significant performance regression:

```cpp
// Baseline: direct function call
void process_direct(ILogger& logger) { logger.log("msg"); }

// Migrated: locator resolve + call
void process_locator(Services& s) { s.resolve<ILogger>().log("msg"); }

// Measure both; resolve overhead should be ~3ns (negligible for most applications)
```

---

## Rollback plan

### Before migration

1. **Tag the pre-migration state** in version control.
2. **Keep globals alive** during transition (don't delete them).
3. **Use a feature flag** if your infrastructure supports it:
   ```cpp
   if (use_service_locator_flag) {
       return services.resolve<ILogger>();
   } else {
       return g_logger;
   }
   ```

### During migration

- Migrate one module at a time.
- Each module can be individually reverted by restoring global access.
- Run tests after each module migration.

### If rollback is needed

1. **Revert call sites** to use globals instead of locator.
2. **Keep the locator infrastructure** (it's not harmful if unused).
3. **Remove locator calls** from the composition root if desired.

### After successful migration

1. **Remove globals** once all call sites use the locator.
2. **Remove feature flag** if used.
3. **Remove transitional `global_services()` accessor** if used.
4. **Update documentation** to reflect the new architecture.

---

## Glossary

| Term | Definition |
|------|------------|
| **Composition root** | The single location (typically `main()` or startup code) where dependencies are wired together |
| **Service** | An object that provides functionality to other parts of the application |
| **Registration** | Adding a service to the locator so it can be resolved |
| **Resolution** | Retrieving a service from the locator by type (and optionally name) |
| **Scope** | A child locator that shadows parent registrations; used for test overrides |
| **Instance registration** | Non-owning registration; caller manages lifetime |
| **Shared registration** | Owning registration via `shared_ptr`; locator shares ownership |
| **Singleton factory** | Factory executed once; result cached and returned on subsequent resolves |
| **Transient factory** | Factory executed on each `createExpected()` call; returns new instance each time |

---

## Next references

- `Documentation/ServiceLocator/Overview - ServiceLocator.md` — Component overview and positioning
- `Documentation/ServiceLocator/User Manual - ServiceLocator.md` — Complete API reference and recipes
- `tests/test_ServiceLocator.cpp` — Test suite demonstrating all patterns
- `benchmarks/benchmark_ServiceLocator.cpp` — Performance measurements
