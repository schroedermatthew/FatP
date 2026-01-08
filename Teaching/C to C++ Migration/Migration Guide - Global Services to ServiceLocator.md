---
doc_id: MG-SERVICELOCATOR-001
doc_type: "Migration Guide"
title: "Global Services to ServiceLocator"
fatp_components: ["ServiceLocator"]
topics: ["C migration", "global state", "singleton pattern", "dependency injection", "service registry", "testability", "scoped overrides"]
constraints: ["global initialization order", "hidden dependencies", "test isolation", "plugin architectures", "multi-tenancy"]
cxx_standard: "C++17"
last_verified: "2025-01-08"
audience: ["C developers", "migration teams", "AI assistants"]
status: "draft"
---

# Migration Guide - Global Services to ServiceLocator

## Scope

This document shows how to migrate C-style global service patterns to explicit dependency management using `ServiceLocator`. It uses SQLite's global configuration system and common C service idioms as case studies, demonstrating that even well-structured global state creates testing, initialization, and modularity problems that explicit service location solves.

## Not Covered

- Full `ServiceLocator` API reference (see User Manual - ServiceLocator)
- Template implementation details (see Companion Guide - ServiceLocator)
- Full dependency injection frameworks (ServiceLocator is a stepping stone, not a DI container)
- Thread pool or async service patterns
- Plugin/DSO boundary considerations (see Design Note - TypeKeyPolicy for DSO)
- Alternative patterns (constructor injection, context objects)

## Prerequisites

- Familiarity with C global state patterns (global structs, singletons, function pointer tables)
- Understanding of C++ templates (basic level)
- Awareness of static initialization order problems
- Familiarity with testing challenges posed by global state
- Access to Fat-P headers

---

## Migration Guide Card

**C Pattern:** Global structs, function pointer tables, singleton accessors  
**Why it fails:** Hidden dependencies; initialization order fiasco; impossible to test in isolation  
**C++ Solution:** `ServiceLocator` — type-safe registry with scoped overrides and explicit lifetime  
**Migration effort:** Medium — requires identifying global dependencies and wiring through locator  
**Verification method:** Compile-time type checking; runtime errors for missing services; test isolation via scopes  
**Incremental migration:** Yes — can migrate one service at a time; global() accessor supports gradual adoption  
**Prerequisites:** None (leaf component, though commonly used with `Expected`)

---

## Table of Contents

1. [The C Patterns](#the-c-patterns)
2. [Why They Fail](#why-they-fail)
3. [The C++ Solution](#the-c-solution)
4. [Migration Mechanics](#migration-mechanics)
5. [Verification](#verification)
6. [Performance Characteristics](#performance-characteristics)
7. [Summary](#summary)
8. [Where It Loses](#where-it-loses)
9. [Read Next](#read-next)

---

## The C Patterns

Four related patterns that C programmers use to provide global services. All share a common weakness: dependencies are implicit and initialization order is fragile. SQLite—the most deployed database engine in the world—demonstrates all these patterns in its configuration system.

### Pattern 1: Global Configuration Struct

**Source:** [SQLite `src/sqliteInt.h`](https://github.com/sqlite/sqlite/blob/master/src/sqliteInt.h) and [SQLite `src/main.c`](https://github.com/sqlite/sqlite/blob/master/src/main.c)

SQLite maintains a single global configuration structure:

```c
/* [Excerpt from src/sqliteInt.h] */
#define SQLITE_WSD
#define GLOBAL(t,v) v
#define sqlite3GlobalConfig sqlite3Config

/* The global configuration is accessed everywhere in the codebase */
```

Configuration is accessed throughout the codebase via the global:

```c
/* [Excerpt from src/main.c - sqlite3_config()] */
case SQLITE_CONFIG_MALLOC: {
    /* EVIDENCE-OF: R-55594-21030 The SQLITE_CONFIG_MALLOC option takes a
    ** single argument which is a pointer to an instance of the
    ** sqlite3_mem_methods structure. */
    sqlite3GlobalConfig.m = *va_arg(ap, sqlite3_mem_methods*);
    break;
}
case SQLITE_CONFIG_GETMALLOC: {
    /* Return the currently defined memory allocation routines. */
    *va_arg(ap, sqlite3_mem_methods*) = sqlite3GlobalConfig.m;
    break;
}
```

**Why programmers use it:** Simple, efficient, single source of truth for configuration.

---

### Pattern 2: Function Pointer Tables (Virtual Dispatch in C)

**Source:** [SQLite `src/os.c`](https://github.com/sqlite/sqlite/blob/master/src/os.c) — VFS (Virtual File System)

SQLite's VFS allows swapping file system implementations:

```c
/* The list of all registered VFS implementations. */
static sqlite3_vfs * SQLITE_WSD vfsList = 0;
#define vfsList GLOBAL(sqlite3_vfs *, vfsList)

/*
** Locate a VFS by name. If no name is given, simply return the
** first VFS on the list.
*/
sqlite3_vfs *sqlite3_vfs_find(const char *zVfs){
    sqlite3_vfs *pVfs = 0;
    /* ... search vfsList ... */
    return pVfs;
}

int sqlite3_vfs_register(sqlite3_vfs *pVfs, int makeDflt){
    /* Add to global list */
}
```

Usage throughout the codebase:

```c
sqlite3_vfs *pVfs = sqlite3_vfs_find("unix");
pVfs->xOpen(pVfs, zFilename, pFile, flags, &outFlags);
```

**Why programmers use it:** Runtime polymorphism without C++ vtables; plugin extensibility.

---

### Pattern 3: Singleton Accessor Functions

**Source:** Common C and C++ pattern

A function guards lazy initialization of a global:

```c
/* Classic C singleton pattern */
static Logger* g_logger = NULL;

Logger* get_logger(void) {
    if (g_logger == NULL) {
        g_logger = create_default_logger();
    }
    return g_logger;
}

void set_logger(Logger* logger) {
    g_logger = logger;
}
```

**Why programmers use it:** Deferred initialization; single access point.

**Problem:** Thread safety requires manual synchronization; order of initialization across compilation units is undefined.

---

### Pattern 4: Callback Registration

**Source:** [SQLite `src/util.c`](https://github.com/sqlite/sqlite/blob/master/src/util.c)

SQLite allows registering callbacks for testing and logging:

```c
/* [Excerpt from src/util.c] */
int sqlite3FaultSim(int iTest){
    int (*xCallback)(int) = sqlite3GlobalConfig.xTestCallback;
    return xCallback ? xCallback(iTest) : SQLITE_OK;
}

/* Logging callback accessed globally */
if( sqlite3GlobalConfig.xLog!=0 ){
    sqlite3GlobalConfig.xLog(sqlite3GlobalConfig.pLogArg, iErrCode, zMsg);
}
```

**Why programmers use it:** Hook points for debugging, testing, and customization.

**Problem:** No scoping—changing the callback affects the entire process.

---

## Why They Fail

The patterns above work in small programs but create four categories of bugs at scale.

### Failure 1: The Static Initialization Order Fiasco

C++ guarantees initialization order within a translation unit but **not across translation units**:

```cpp
// file_a.cpp
ConfigService g_config;  // Initialized when?

// file_b.cpp
Logger g_logger(&g_config);  // Uses g_config - but is it ready?

// The order depends on link order, which is undefined
```

**Real impact:** Crashes on startup that only appear in certain build configurations or platforms.

**CWE-456 (Missing Initialization):** Global dependencies can access uninitialized state.

---

### Failure 2: Hidden Dependencies Make Testing Impossible

When services access globals directly, unit testing requires mocking the entire global state:

```cpp
// Production code
void process_request(Request& req) {
    Logger& log = get_logger();         // Hidden dependency #1
    Database& db = get_database();      // Hidden dependency #2
    Config& cfg = get_config();         // Hidden dependency #3
    
    log.info("Processing");
    auto result = db.query(cfg.get("query"));
    // ...
}

// Test code - must somehow mock all three globals
void test_process_request() {
    // Problem: How do we inject a test logger? Test database?
    // Answer: Usually we can't, so we don't write the test.
}
```

**Real impact:** Code becomes untestable; bugs ship because tests are too hard to write.

---

### Failure 3: Global State Prevents Multi-Tenancy

A single global means one configuration for the entire process:

```c
/* SQLite global config affects ALL databases in the process */
sqlite3_config(SQLITE_CONFIG_MALLOC, &myAllocator);

/* Now every sqlite3 connection uses myAllocator
   - even connections owned by unrelated libraries
   - even connections that need different behavior */
```

**Real impact:** Libraries using SQLite conflict with application configuration; embedding multiple versions is impossible.

---

### Failure 4: Initialization Races in Multithreaded Code

Lazy initialization without synchronization causes data races:

```cpp
// Broken double-checked locking (pre-C++11)
Logger* get_logger() {
    if (g_logger == nullptr) {           // Thread A reads null
        // Thread A suspended here
        // Thread B also reads null, creates logger
        // Thread A resumes, creates second logger
        g_logger = new Logger();         // Double initialization!
    }
    return g_logger;
}
```

**CWE-362 (Race Condition):** Concurrent first-access creates multiple instances or returns partially-constructed objects.

Even with proper synchronization, every access pays the cost:

```cpp
Logger* get_logger() {
    std::lock_guard<std::mutex> lock(g_mutex);  // Every call!
    if (g_logger == nullptr) {
        g_logger = new Logger();
    }
    return g_logger;
}
```

---

## The C++ Solution

`ServiceLocator` addresses all four failure modes through:

1. **Explicit registration** — Services are registered before use; no initialization order problems
2. **Type-safe resolution** — Compile-time type checking; no stringly-typed lookups
3. **Scoped overrides** — Child locators can override services without affecting parent
4. **Thread-safe singleton factories** — Exactly-once initialization with proper synchronization

### Core API

```cpp
#include "ServiceLocator.h"
using namespace fat_p;

// Types
DefaultServiceLocator locator;           // Single-threaded
ThreadSafeServiceLocator safeLocator;    // Thread-safe

// Registration (three ownership models)
locator.registerInstance<ILogger>(myLogger);              // Non-owning
locator.registerShared<ILogger>(std::make_shared<...>()); // Shared ownership
locator.registerFactory<ILogger>(                         // Lazy creation
    []() { return std::make_unique<FileLogger>(); },
    ServiceLifetime::Singleton);                          // or Transient

// Resolution
ILogger* ptr = locator.tryResolve<ILogger>();             // Returns nullptr if missing
ILogger& ref = locator.resolve<ILogger>();                // Throws if missing
auto result = locator.resolveExpected<ILogger>();         // Returns Expected<ref, Error>

// Scoped overrides
{
    auto scope = locator.makeScope();
    scope.locator().registerInstance<ILogger>(testLogger);
    // Resolves in scope.locator() see testLogger
    // Resolves in parent locator still see original
}
// testLogger automatically unregistered
```

### Named Services

Multiple implementations of the same interface:

```cpp
locator.registerInstance<IDatabase>(primaryDb, "primary");
locator.registerInstance<IDatabase>(replicaDb, "replica");

IDatabase& primary = locator.resolve<IDatabase>("primary");
IDatabase& replica = locator.resolve<IDatabase>("replica");
```

### Factory Lifetimes

```cpp
// Singleton: Created once, cached forever
locator.registerFactory<ExpensiveService>(
    []() { return std::make_unique<ExpensiveService>(); },
    ServiceLifetime::Singleton);

// Transient: New instance per createExpected() call
locator.registerFactory<RequestHandler>(
    []() { return std::make_unique<RequestHandler>(); },
    ServiceLifetime::Transient);

// Singleton resolution (same instance)
ExpensiveService& s1 = locator.resolve<ExpensiveService>();
ExpensiveService& s2 = locator.resolve<ExpensiveService>();
assert(&s1 == &s2);  // Same instance

// Transient creation (different instances)
auto h1 = locator.createExpected<RequestHandler>();
auto h2 = locator.createExpected<RequestHandler>();
assert(h1.value().get() != h2.value().get());  // Different instances
```

### Thread Safety

```cpp
ThreadSafeServiceLocator locator;

// Concurrent resolution is safe
std::thread t1([&]() { locator.resolve<ILogger>(); });
std::thread t2([&]() { locator.resolve<ILogger>(); });

// Singleton factories are synchronized
// Only one thread executes the factory; others wait
locator.registerFactory<Widget>(
    []() {
        std::this_thread::sleep_for(100ms);  // Slow creation
        return std::make_unique<Widget>();
    },
    ServiceLifetime::Singleton);

// 16 threads resolve simultaneously - factory runs exactly once
```

### Circular Dependency Detection

```cpp
locator.registerFactory<ServiceA>(
    [&locator]() {
        // Oops - ServiceA's factory tries to resolve ServiceA
        locator.resolve<ServiceA>();  // Detected!
        return std::make_unique<ServiceA>();
    },
    ServiceLifetime::Singleton);

auto result = locator.resolveExpected<ServiceA>();
// result.error().mCode == ServiceError::CircularDependency
```

---

## Migration Mechanics

### Step 1: Identify Global Services

Audit your codebase for global state patterns:

```bash
# Find global variables
grep -r "^static.*\*.*=" --include="*.c" --include="*.cpp"

# Find singleton accessors
grep -r "get_.*instance\|getInstance\|Get.*Singleton" --include="*.h"

# Find global function pointers
grep -r "typedef.*(\*.*)" --include="*.h"
```

Create an inventory:

| Service | Current Pattern | Dependencies | Thread-Safe? |
|---------|----------------|--------------|--------------|
| Logger | Singleton function | Config | No |
| Config | Global struct | None | No |
| Database | Global pointer | Logger, Config | Manual |
| Allocator | Function pointer table | None | Yes |

---

### Step 2: Define Service Interfaces

Abstract the interface if not already done:

```cpp
// Before: Concrete global
extern Logger g_logger;
void log_message(const char* msg) {
    g_logger.write(msg);
}

// After: Interface
class ILogger {
public:
    virtual ~ILogger() = default;
    virtual void write(std::string_view msg) = 0;
};
```

---

### Step 3: Create the Locator

For gradual migration, use a global locator initially:

```cpp
// services.h
#pragma once
#include "ServiceLocator.h"

inline fat_p::ThreadSafeServiceLocator& services() {
    return fat_p::ThreadSafeServiceLocator::global();
}
```

---

### Step 4: Register Services at Startup

Replace scattered global initialization with explicit registration:

```cpp
// Before: Scattered initialization
// file_a.cpp
static Logger g_logger;  // When does this run?

// file_b.cpp
void init_database() {
    g_database = new Database(&g_logger);  // Is g_logger ready?
}

// After: Explicit, ordered initialization
// main.cpp
int main() {
    auto& loc = services();
    
    // Order is explicit and visible
    auto logger = std::make_shared<FileLogger>("/var/log/app.log");
    loc.registerShared<ILogger>(logger);
    
    loc.registerFactory<IDatabase>(
        [&loc]() {
            return std::make_unique<Database>(loc.resolve<ILogger>());
        },
        ServiceLifetime::Singleton);
    
    // Application code
    run_application();
}
```

---

### Step 5: Replace Global Access with Resolution

Convert global access to service resolution:

```cpp
// Before: Hidden dependency
void process_request(Request& req) {
    g_logger.info("Processing");
    g_database.query(req.sql);
}

// After: Explicit dependency (option A - resolve at use)
void process_request(Request& req, ServiceLocator& loc) {
    loc.resolve<ILogger>().write("Processing");
    loc.resolve<IDatabase>().query(req.sql);
}

// After: Explicit dependency (option B - resolve at construction)
class RequestProcessor {
    ILogger& mLog;
    IDatabase& mDb;
public:
    explicit RequestProcessor(ServiceLocator& loc)
        : mLog(loc.resolve<ILogger>())
        , mDb(loc.resolve<IDatabase>()) {}
    
    void process(Request& req) {
        mLog.write("Processing");
        mDb.query(req.sql);
    }
};
```

---

### Step 6: Use RAII Registration for Tests

```cpp
TEST_CASE(process_request_logs_message) {
    // Create isolated test environment
    auto scope = services().makeScope();
    auto& loc = scope.locator();
    
    // Register test doubles
    MockLogger mockLog;
    MockDatabase mockDb;
    loc.registerInstance<ILogger>(mockLog);
    loc.registerInstance<IDatabase>(mockDb);
    
    // Test with injected dependencies
    RequestProcessor proc(loc);
    Request req{.sql = "SELECT 1"};
    proc.process(req);
    
    // Verify
    ASSERT_TRUE(mockLog.hasMessage("Processing"));
    ASSERT_TRUE(mockDb.wasQueried("SELECT 1"));
    
    // Cleanup automatic - scope destructor unregisters
}
```

---

### Migration Pattern: Gradual Adoption

For large codebases, migrate incrementally:

```cpp
// Phase 1: Wrap existing global in locator
static Logger g_legacy_logger;  // Keep for now

void init_services() {
    // Register the existing global
    services().registerInstance<ILogger>(g_legacy_logger);
}

// Phase 2: New code uses locator
void new_feature() {
    auto& log = services().resolve<ILogger>();
    // ...
}

// Phase 3: Legacy code updated gradually
// Old: g_logger.write(msg);
// New: services().resolve<ILogger>().write(msg);

// Phase 4: Remove global when all code migrated
```

---

## Verification

### Compile-Time Verification

Service type mismatches are caught at compile time:

```cpp
locator.registerInstance<ILogger>(myLogger);
locator.resolve<IDatabase>();  // Compiles, but returns error at runtime

// With Expected pattern:
auto result = locator.resolveExpected<IDatabase>();
if (!result.has_value()) {
    // ServiceError::ServiceNotFound
}
```

### Runtime Verification

Missing services produce clear errors:

```cpp
auto result = locator.resolveExpected<IUnregistered>();
// result.error().mCode == ServiceError::ServiceNotFound
// result.error().fullMessage() == "Service not found: No matching service registration"
```

### Test Isolation Verification

```cpp
TEST_CASE(scopes_isolate_changes) {
    auto& parent = services();
    parent.registerInstance<IConfig>(prodConfig);
    
    {
        auto scope = parent.makeScope();
        scope.locator().registerInstance<IConfig>(testConfig);
        
        // Child sees override
        ASSERT_EQ(&scope.locator().resolve<IConfig>(), &testConfig);
        
        // Parent unchanged
        ASSERT_EQ(&parent.resolve<IConfig>(), &prodConfig);
    }
    
    // After scope: parent still has original
    ASSERT_EQ(&parent.resolve<IConfig>(), &prodConfig);
}
```

### Example Test: Singleton Factory Exactly-Once

```cpp
TEST_CASE(concurrent_singleton_exactly_once) {
    ThreadSafeServiceLocator locator;
    
    std::atomic<int> factoryInvocations{0};

    locator.registerFactory<Widget>(
        [&factoryInvocations]() -> std::unique_ptr<Widget> {
            factoryInvocations.fetch_add(1);
            std::this_thread::sleep_for(10ms);  // Widen race window
            return std::make_unique<Widget>(42);
        },
        ServiceLifetime::Singleton);

    constexpr int kThreads = 16;
    std::vector<std::thread> threads;
    std::atomic<Widget*> firstObserved{nullptr};
    std::atomic<int> mismatchCount{0};

    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back([&]() {
            Widget* p = locator.tryResolve<Widget>();
            Widget* expected = nullptr;
            if (!firstObserved.compare_exchange_strong(expected, p)) {
                if (expected != p) mismatchCount++;
            }
        });
    }

    for (auto& th : threads) th.join();

    ASSERT_EQ(factoryInvocations.load(), 1);  // Exactly once
    ASSERT_EQ(mismatchCount.load(), 0);       // Same instance
}
```

---

## Performance Characteristics

### Resolution Cost

| Operation | DefaultServiceLocator | ThreadSafeServiceLocator |
|-----------|----------------------|--------------------------|
| tryResolve (hit) | ~15-25 ns | ~40-60 ns (shared lock) |
| tryResolve (miss) | ~10-15 ns | ~30-40 ns |
| resolve (hit) | ~15-25 ns | ~40-60 ns |
| registerInstance | ~50-100 ns | ~80-150 ns (exclusive lock) |
| registerFactory | ~100-200 ns | ~150-300 ns |

### When Resolution Cost Matters

ServiceLocator is designed for **infrequent resolution**:

```cpp
// BAD: Resolve in hot loop
for (int i = 0; i < 1000000; ++i) {
    locator.resolve<ILogger>().write("msg");  // 40ns * 1M = 40ms overhead
}

// GOOD: Resolve once, use reference
ILogger& log = locator.resolve<ILogger>();
for (int i = 0; i < 1000000; ++i) {
    log.write("msg");  // Zero overhead
}
```

### Singleton Factory Synchronization

First resolution of a singleton factory includes creation cost:

```cpp
// First call: factory execution + ~100ns synchronization
Widget& w1 = locator.resolve<Widget>();

// Subsequent calls: ~40ns (just hash lookup + shared lock)
Widget& w2 = locator.resolve<Widget>();
```

### Memory Overhead

| Component | Overhead |
|-----------|----------|
| Per ServiceLocator | ~64 bytes + hash table overhead |
| Per Instance registration | ~56 bytes |
| Per Shared registration | ~72 bytes (includes shared_ptr) |
| Per Factory registration | ~96 bytes (includes std::function) |
| Per Singleton state (if used) | ~80 bytes (mutex + cv + shared_ptr) |

---

## Summary

### The Core Tradeoff

| Aspect | C Global State | ServiceLocator |
|--------|---------------|----------------|
| Simplicity | Direct access | Resolution call |
| Dependencies | Hidden | Explicit |
| Initialization order | Fragile | Controlled |
| Testability | Poor | Good (scoped overrides) |
| Thread safety | Manual | Policy-based |
| Performance | Zero overhead | ~20-60 ns per resolution |

### What You Gain

1. **Explicit dependencies** — No more hidden couplings
2. **Testability** — Scoped overrides enable isolated unit tests
3. **Thread safety** — Singleton factories synchronized correctly
4. **Initialization order** — Explicit registration order, no fiasco
5. **Multi-tenancy** — Different scopes can have different configurations

### What You Pay

1. **Resolution overhead** — ~20-60 ns per resolution (resolve once, use reference)
2. **Code verbosity** — Must wire services through locator
3. **Runtime errors** — Missing services fail at runtime, not compile time

---

## Where It Loses

### 1. When Zero Overhead is Required

For services resolved millions of times per second in hot paths, ServiceLocator adds measurable overhead. Use direct references instead:

```cpp
// Resolve once at startup
ILogger& log = locator.resolve<ILogger>();

// Pass reference to hot code
void hot_path(ILogger& log) {
    log.write("msg");  // Zero overhead
}
```

### 2. When Compile-Time Guarantees are Needed

ServiceLocator errors are detected at runtime. For compile-time enforcement of service availability, use constructor injection:

```cpp
// Compile-time: Missing dependency = compile error
class Widget {
public:
    Widget(ILogger& log, IDatabase& db) : mLog(log), mDb(db) {}
private:
    ILogger& mLog;
    IDatabase& mDb;
};
```

### 3. When Services Cross DSO Boundaries

The default `TypeKeyPolicy` uses address identity, which is not stable across shared library boundaries. Plugins loaded at runtime may get different type IDs. For plugin architectures, implement a custom `TypeKeyPolicy` using string names or GUIDs.

### 4. When Full Dependency Injection is Needed

ServiceLocator is a **service locator**, not a **dependency injection container**. It doesn't automatically construct objects with their dependencies. For complex object graphs with automatic wiring, consider a full DI framework—but note that ServiceLocator can be a stepping stone during migration.

---

## Read Next

- **User Manual - ServiceLocator**: Complete API reference and usage patterns
- **Companion Guide - ServiceLocator**: Design rationale and policy details
- **Design Note - TypeKeyPolicy for DSO**: Handling plugin/shared library boundaries
- **Migration Guide - Error Handling Patterns to Expected**: For the `Expected<T,E>` return types
- **Case Study - The Static Initialization Order Fiasco**: Detailed investigation of initialization bugs
