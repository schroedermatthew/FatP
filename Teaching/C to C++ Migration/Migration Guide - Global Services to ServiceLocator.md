---
doc_id: MG-SERVICELOCATOR-001
doc_type: "Migration Guide"
title: "Global Services to ServiceLocator"
from_pattern: "Global structs, singletons, function pointer tables"
to_component: "ServiceLocator"
fatp_version: "1.0"
cxx_standard: "C++17"
std_equivalent: null
std_since: null
boost_equivalent: null
migration_complexity: "Medium"
breaking_changes: true
last_verified: "2025-01-09"
---

# Migration Guide - Global Services to ServiceLocator

### *From Hidden Dependencies to Explicit, Testable Service Management*

*FAT-P Library — January 2025*

---

## Migration Card

| Aspect | Detail |
|--------|--------|
| **C Pattern** | Global structs, singleton accessors, function pointer tables, callback registration |
| **Problems Solved** | Static initialization order fiasco, hidden dependencies, test isolation, thread safety |
| **Fat-P Component** | `ServiceLocator<TypeKeyPolicy>` |
| **Migration Complexity** | Medium — requires identifying dependencies and wiring through locator |
| **Runtime Overhead** | ~5-10ns lookup (hash table); zero for cached references |
| **Breaking Changes** | Yes — API changes from global access to explicit locator |

---

## Alternatives

No standard or Boost equivalent exists for the service locator pattern. However, other approaches solve the same underlying problem—decoupling from globals, enabling testability. This section presents the viable paths so you can choose the right tool.

### Choosing Your Approach

| Approach | Model | Best When |
|----------|-------|-----------|
| **ServiceLocator (Fat-P)** | Pull | Deep call stacks, runtime substitution needed |
| **Boost.DI** | Push | Compile-time verification wanted, framework buy-in acceptable |
| **Manual constructor injection** | Push | Small codebase, few dependencies |

### Approach 1: ServiceLocator (Fat-P)

Services are registered in a central registry and pulled by consumers at runtime. Dependencies flow through function parameters as a single locator reference.

```cpp
#include "ServiceLocator.h"

struct ILogger {
    virtual ~ILogger() = default;
    virtual void log(std::string_view msg) = 0;
};

struct IDatabase {
    virtual ~IDatabase() = default;
    virtual void query(std::string_view sql) = 0;
};

// Registration at startup
void setup(ServiceLocator& services) {
    services.register_service<ILogger>(std::make_shared<FileLogger>());
    services.register_service<IDatabase>(std::make_shared<PostgresDB>());
}

// Consumer pulls what it needs
void process_order(ServiceLocator& services, const Order& order) {
    auto logger = services.require<ILogger>();
    auto db = services.require<IDatabase>();
    
    logger->log("Processing order");
    db->query("INSERT INTO orders ...");
}

// Testing: scoped override
TEST(OrderProcessing, LogsCorrectly) {
    ServiceLocator services;
    auto mock_logger = std::make_shared<MockLogger>();
    auto mock_db = std::make_shared<MockDatabase>();
    
    services.register_service<ILogger>(mock_logger);
    services.register_service<IDatabase>(mock_db);
    
    process_order(services, Order{});
    EXPECT_TRUE(mock_logger->received("Processing order"));
}
```

**Pros:** Single parameter threads through deep call stacks. Runtime service substitution. Scoped overrides for testing.

**Cons:** ~5-10ns lookup overhead. Dependencies not visible in constructor signatures.

### Approach 2: Boost.DI

Dependencies are declared in constructors and injected automatically by the framework. The injector builds the object graph at composition root.

```cpp
#include <boost/di.hpp>
namespace di = boost::di;

struct ILogger {
    virtual ~ILogger() = default;
    virtual void log(std::string_view msg) = 0;
};

struct IDatabase {
    virtual ~IDatabase() = default;
    virtual void query(std::string_view sql) = 0;
};

class FileLogger : public ILogger { /* ... */ };
class PostgresDB : public IDatabase { /* ... */ };

// Consumer declares dependencies in constructor
class OrderProcessor {
    std::shared_ptr<ILogger> mLogger;
    std::shared_ptr<IDatabase> mDatabase;
public:
    OrderProcessor(std::shared_ptr<ILogger> logger, 
                   std::shared_ptr<IDatabase> db)
        : mLogger(std::move(logger))
        , mDatabase(std::move(db)) 
    {}
    
    void process(const Order& order) {
        mLogger->log("Processing order");
        mDatabase->query("INSERT INTO orders ...");
    }
};

// Composition root: configure bindings
int main() {
    auto injector = di::make_injector(
        di::bind<ILogger>.to<FileLogger>(),
        di::bind<IDatabase>.to<PostgresDB>()
    );
    
    auto processor = injector.create<OrderProcessor>();
    processor.process(Order{});
}

// Testing: create with mocks directly
TEST(OrderProcessing, LogsCorrectly) {
    auto mock_logger = std::make_shared<MockLogger>();
    auto mock_db = std::make_shared<MockDatabase>();
    
    OrderProcessor processor(mock_logger, mock_db);
    processor.process(Order{});
    EXPECT_TRUE(mock_logger->received("Processing order"));
}
```

**Pros:** Compile-time wiring verification. Dependencies explicit in constructors. Zero runtime lookup overhead.

**Cons:** Framework buy-in required. Learning curve. Harder to do runtime substitution.

### Approach 3: Manual Constructor Injection

No framework—just pass dependencies explicitly through constructors and function parameters. The simplest approach when you have few dependencies.

```cpp
struct ILogger {
    virtual ~ILogger() = default;
    virtual void log(std::string_view msg) = 0;
};

struct IDatabase {
    virtual ~IDatabase() = default;
    virtual void query(std::string_view sql) = 0;
};

// Option A: Class holds dependencies
class OrderProcessor {
    ILogger& mLogger;
    IDatabase& mDatabase;
public:
    OrderProcessor(ILogger& logger, IDatabase& db)
        : mLogger(logger), mDatabase(db) 
    {}
    
    void process(const Order& order) {
        mLogger.log("Processing order");
        mDatabase.query("INSERT INTO orders ...");
    }
};

// Option B: Function takes dependencies
void process_order(ILogger& logger, IDatabase& db, const Order& order) {
    logger.log("Processing order");
    db.query("INSERT INTO orders ...");
}

// Composition root: wire manually
int main() {
    FileLogger logger("/var/log/app.log");
    PostgresDB database(connection_string);
    
    OrderProcessor processor(logger, database);
    processor.process(Order{});
    
    // Or function style:
    process_order(logger, database, Order{});
}

// Testing: pass mocks directly
TEST(OrderProcessing, LogsCorrectly) {
    MockLogger mock_logger;
    MockDatabase mock_db;
    
    process_order(mock_logger, mock_db, Order{});
    EXPECT_TRUE(mock_logger.received("Processing order"));
}
```

**Pros:** No framework. No overhead. Dependencies completely explicit. Easy to understand.

**Cons:** Parameter lists grow as dependencies increase. Deep call stacks require threading many parameters. No scoped override mechanism.

### When to Choose What

**Choose ServiceLocator when:**
- Call stacks are deep (5+ levels)
- You need runtime service substitution (plugins, A/B testing)
- Parameter lists are becoming unwieldy
- Scoped overrides simplify testing

**Choose Boost.DI when:**
- You want compile-time verification of the dependency graph
- Your team is comfortable with DI framework conventions
- Object construction is concentrated at composition root
- Runtime substitution is not needed

**Choose manual injection when:**
- Codebase is small (<10k lines)
- You have few services (3-5)
- Call stacks are shallow
- You want zero framework dependencies

---

## Table of Contents

1. [Alternatives](#alternatives)
2. [The Problem with Global Services](#the-problem-with-global-services)
3. [Real-World Global State Disasters](#real-world-global-state-disasters)
4. [The C Patterns](#the-c-patterns)
5. [The ServiceLocator Solution](#the-servicelocator-solution)
6. [Migration Steps](#migration-steps)
7. [Before/After Examples](#beforeafter-examples)
8. [Advanced Patterns](#advanced-patterns)
9. [Verification](#verification)
10. [When ServiceLocator Loses](#when-servicelocator-loses)
11. [Summary](#summary)

---

## The Problem with Global Services

Global state is seductive. When every module needs access to logging, configuration, or a database connection, making them global seems like the simplest solution. No need to pass parameters around. No need to think about who owns what. Just reach out and grab what you need:

```c
/* Logging is needed everywhere, so make it global */
static Logger* g_logger = NULL;

void log_message(const char* msg) {
    if (g_logger) {
        g_logger->write(msg);
    }
}

/* Configuration too */
static Config g_config;

/* And the database connection */
static Database* g_db = NULL;
```

The simplicity is illusory. Global state creates problems that compound as the codebase grows:

| Problem | Consequence |
|---------|-------------|
| **Initialization order** | `g_db` uses `g_logger` before `g_logger` is initialized |
| **Test isolation** | Can't test module A without module B's global state |
| **Thread safety** | Who initialized the singleton? Was it thread-safe? |
| **Hidden dependencies** | Function signatures lie—`process()` secretly uses 5 globals |
| **Lifetime management** | When do globals get destroyed? In what order? |

The function `void process_order(const Order& order)` looks like it depends only on an order. In reality, it might access a global logger, a global database, a global configuration, a global cache, and a global metrics collector. You can't understand what the function does without reading every line of its implementation. You can't test it without setting up all that global state. You can't safely modify it without understanding all the hidden connections.

---

## Real-World Global State Disasters

These aren't theoretical concerns. Global state causes real bugs in production systems.

### The SQLite Global Configuration

SQLite—the most deployed database in the world—uses global state extensively:

```c
/* From sqlite3.c */
SQLITE_PRIVATE SQLITE_WSD struct Sqlite3Config sqlite3Config = {
   SQLITE_DEFAULT_MEMSTATUS,   /* bMemstat */
   1,                          /* bCoreMutex */
   SQLITE_THREADSAFE==1,       /* bFullMutex */
   /* ... 50+ more fields ... */
};
```

This global configuration affects the entire process. You can't have different configurations for different connections—every connection shares the same mutex settings, memory allocator, and VFS implementation. You must call `sqlite3_config()` before opening your first database connection, and the order of calls matters. Tests can't run in isolation because they all share the same global configuration.

### The Static Initialization Order Fiasco

C++ provides no guarantee about the order in which global objects are initialized across translation units. When globals depend on each other, you're gambling on link order:

```cpp
// file_a.cpp
Logger g_logger;  // Order 1 or 2?

// file_b.cpp  
Database g_db(g_logger);  // Uses g_logger - is it initialized?

// The C++ standard provides NO guarantee about initialization order
// across translation units. This is undefined behavior waiting to happen.
```

The symptoms are maddening. The program crashes on startup with an uninitialized pointer—sometimes. It works in Debug but crashes in Release because the linker chose a different order. It works with GCC but crashes with MSVC. The bug appears and disappears based on factors that seem unrelated to the code you're changing.

### The Singleton Race

C++11 guarantees thread-safe initialization of function-local statics, but that guarantee is narrower than it appears:

```cpp
Logger* get_logger() {
    static Logger instance;  // C++11 guarantees thread-safety... 
    return &instance;        // ...but what about Logger's constructor?
}

// If Logger's constructor accesses another singleton:
Logger::Logger() {
    auto config = get_config();  // DEADLOCK or crash
    // get_config() might be waiting for get_logger() to complete
}
```

The Logger constructor calls `get_config()`. If another thread is already constructing the Config singleton and Config's constructor calls `get_logger()`, you have a deadlock. Or if the call graph is more complex, you get half-constructed objects and undefined behavior.

---

## The C Patterns

Before migrating, we need to recognize the patterns we're replacing. Global state in C takes several forms, each with its own pathologies.

### Pattern 1: Global Configuration Struct

The "god struct" pattern collects all configuration into one global structure. Everything accesses it directly:

```c
struct GlobalConfig {
    int log_level;
    char* db_path;
    int max_connections;
    void (*error_handler)(int, const char*);
    /* ... grows forever ... */
};

static struct GlobalConfig g_config = {
    .log_level = LOG_INFO,
    .db_path = "/var/db/app.db",
    /* ... */
};

/* Accessed everywhere */
void process_request(Request* req) {
    if (g_config.log_level >= LOG_DEBUG) {
        log_request(req);
    }
    Database* db = connect(g_config.db_path);
    /* ... */
}
```

The struct accumulates fields over time—every new feature adds more. Modifications aren't thread-safe. Testing requires mutating the global and hoping you remember to restore it. The function signature says nothing about what configuration it actually uses.

### Pattern 2: Singleton Accessor Functions

When lazy initialization is needed, a global pointer with a `pthread_once` guard provides thread-safe initialization:

```c
static Logger* g_logger = NULL;
static pthread_once_t g_logger_once = PTHREAD_ONCE_INIT;

static void init_logger(void) {
    g_logger = create_logger();
}

Logger* get_logger(void) {
    pthread_once(&g_logger_once, init_logger);
    return g_logger;
}
```

Initialization is thread-safe, but there's no cleanup—the logger lives until process exit. There's no way to override the logger for testing. And every function that calls `get_logger()` has a hidden dependency that its signature doesn't reveal.

### Pattern 3: Function Pointer Tables

SQLite's VFS pattern uses a struct of function pointers, with a global pointer to the current implementation:

```c
struct FileSystem {
    int (*open)(const char* path, int flags);
    int (*read)(int fd, void* buf, size_t n);
    int (*write)(int fd, const void* buf, size_t n);
    int (*close)(int fd);
};

static struct FileSystem* g_fs = &default_fs;

void set_filesystem(struct FileSystem* fs) {
    g_fs = fs;  /* Global mutation! */
}
```

This enables pluggable implementations—you can swap in a test filesystem. But the swap affects all callers in the entire process. There's no way to have different filesystems for different components running concurrently.

### Pattern 4: Callback Registration

Error handlers and other callbacks often use global registration:

```c
typedef void (*ErrorCallback)(int code, const char* msg);
static ErrorCallback g_error_callback = default_error_handler;

void set_error_callback(ErrorCallback cb) {
    g_error_callback = cb;
}

void report_error(int code, const char* msg) {
    if (g_error_callback) {
        g_error_callback(code, msg);
    }
}
```

Setting a callback affects the entire process. You can't have different error handlers for different subsystems. In a library, your callback might get overwritten by another library or by the application.

---

## The ServiceLocator Solution

The fundamental insight behind `ServiceLocator` is that services should be explicit. Instead of reaching into global state, functions declare what services they need. Instead of hoping globals are initialized in the right order, you register services explicitly in your main function. Instead of mutating globals for tests, you provide mock implementations through the same interface.

ServiceLocator is a type-safe registry that maps service interfaces to implementations. You register services at startup, then pass the locator to functions that need them. The locator provides lookup by type—ask for an ILogger and get the registered ILogger implementation:

```cpp
#include "ServiceLocator.h"
using namespace fat_p;

// Define service interfaces
struct ILogger {
    virtual ~ILogger() = default;
    virtual void log(std::string_view msg) = 0;
};

struct IDatabase {
    virtual ~IDatabase() = default;
    virtual void query(std::string_view sql) = 0;
};

// Register implementations at startup
void setup_services(ServiceLocator& locator) {
    locator.register_service<ILogger>(std::make_shared<FileLogger>("/var/log/app.log"));
    locator.register_service<IDatabase>(std::make_shared<PostgresDB>(connection_string));
}

// Use services explicitly
void process_request(ServiceLocator& locator, const Request& req) {
    auto logger = locator.get<ILogger>();
    auto db = locator.get<IDatabase>();
    
    logger->log("Processing request");
    db->query(req.sql);
}
```

The `process_request` function signature now tells the truth: it needs a ServiceLocator (and thus access to services) and a Request. You can see the dependencies without reading the implementation. You can test it by providing a locator with mock implementations. The initialization order is explicit—you control when services are registered.

### Scoped Overrides for Testing

The `override_service` method returns an RAII guard that temporarily replaces a service. When the guard goes out of scope, the original service is restored. Tests can swap in mocks without affecting other tests:

```cpp
{
    auto guard = services.override_service<ILogger>(mock_logger);
    // mock_logger is now returned by get<ILogger>()
    run_test();
}  // Original logger restored
```

### Lifetime Control

Services are destroyed in reverse registration order. If the database was registered after the logger, it will be destroyed before the logger—so the database destructor can still log. This is the opposite of C++ global destruction order, which is often exactly wrong.

---

## Migration Steps

Migration from global state to ServiceLocator happens in phases. You can migrate incrementally, with legacy and modern code coexisting during the transition.

### Step 1: Identify Global Dependencies

Before you can replace globals, you need to find them all. Search for global variables, singleton patterns, and hidden dependencies:

```bash
# Find global variables
grep -rn "^static.*\*.*=" src/
grep -rn "^[A-Z].*g_" src/

# Find singleton patterns
grep -rn "getInstance\|get_instance\|GetInstance" src/
```

Create an inventory: each global, what it provides, and what other globals it depends on. This map reveals the initialization order you'll need to encode in your service registration.

### Step 2: Define Service Interfaces

For each global, create an abstract interface. The interface captures what callers need, without exposing implementation details. A global logger becomes an ILogger interface:

```c
// Before: global logger
static FILE* g_log_file = NULL;
void log_message(const char* msg) {
    if (g_log_file) fprintf(g_log_file, "%s\n", msg);
}
```

```cpp
// After: abstract interface
struct ILogger {
    virtual ~ILogger() = default;
    virtual void log(std::string_view msg) = 0;
};

// Concrete implementation
class FileLogger : public ILogger {
    FILE* mFile;
public:
    explicit FileLogger(const char* path) : mFile(fopen(path, "a")) {}
    ~FileLogger() { if (mFile) fclose(mFile); }
    void log(std::string_view msg) override {
        if (mFile) fprintf(mFile, "%.*s\n", (int)msg.size(), msg.data());
    }
};
```

The interface enables testing with mocks. The concrete implementation can change without affecting callers.

### Step 3: Create Central Registration

Centralize service creation in your main function. The order of registration is the order of initialization—explicit and visible:

```cpp
// services.h
ServiceLocator& get_services();

// services.cpp
ServiceLocator& get_services() {
    static ServiceLocator instance;
    return instance;
}

// main.cpp
int main() {
    auto& services = get_services();
    services.register_service<ILogger>(std::make_shared<FileLogger>("/var/log/app.log"));
    services.register_service<IDatabase>(std::make_shared<PostgresDB>(db_url));
    
    run_application(services);
}
```

### Step 4: Thread Services Through Functions

Replace hidden global access with explicit service lookup. The function signature now declares its dependencies:

```cpp
// Before: hidden globals
void process_order(const Order& order) {
    log_message("Processing order");  // Hidden global
    auto* db = get_database();        // Hidden global
    db->insert(order);
}

// After: explicit dependencies
void process_order(ServiceLocator& services, const Order& order) {
    services.require<ILogger>()->log("Processing order");
    services.require<IDatabase>()->insert(order);
}
```

This is the bulk of the migration work. Each function that accessed globals needs to receive the ServiceLocator.

### Step 5: Add Test Overrides

With services going through the locator, tests can substitute mock implementations:

```cpp
TEST_CASE(process_order_logs_correctly) {
    auto& services = get_services();
    
    auto mock_logger = std::make_shared<MockLogger>();
    auto mock_db = std::make_shared<MockDatabase>();
    
    // Scoped overrides - restored automatically
    auto logger_override = services.override_service<ILogger>(mock_logger);
    auto db_override = services.override_service<IDatabase>(mock_db);
    
    Order order{...};
    process_order(services, order);
    
    ASSERT_TRUE(mock_logger->received("Processing order"));
    ASSERT_TRUE(mock_db->inserted(order));
}
// Original services restored here
```

The scoped override pattern ensures tests don't pollute each other. When the override guard goes out of scope, the original service is restored.

### Step 6: Handle Legacy Code Gradually

You don't have to migrate everything at once. Provide global accessor functions that delegate to the ServiceLocator:

```cpp
// Transition helper - remove once migration complete
ILogger& global_logger() {
    return *get_services().require<ILogger>();
}

// Legacy code can still work:
void legacy_function() {
    global_logger().log("Still works during migration");
}
```

As you migrate functions to take the ServiceLocator directly, you can track which code still uses the global accessors. When nothing references them, remove them.

---

## Before/After Examples

These examples show complete transformations of realistic global state patterns to ServiceLocator.

### Example 1: Application Initialization

The static initialization order fiasco is one of the most insidious C++ bugs. Globals that depend on each other are initialized in an order the standard doesn't define. In this example, the database uses the logger before the logger is constructed:

```cpp
// globals.cpp - order is undefined!
Logger g_logger;
Config g_config;
Database g_db(g_config.db_path);  // g_config might not be initialized!

void init_app() {
    g_logger.init("/var/log/app.log");
    g_config.load("/etc/app.conf");
    g_db.connect();  // What if this fails?
}
```

The ServiceLocator version makes initialization order explicit. Each service is registered after its dependencies. Errors are propagated rather than silently ignored:

```cpp
Expected<ServiceLocator, std::string> init_app() {
    ServiceLocator services;
    
    auto config = Config::load("/etc/app.conf");
    if (!config) return unexpected("Config: " + config.error());
    services.register_service<IConfig>(std::make_shared<Config>(std::move(*config)));
    
    auto logger = FileLogger::create(services.require<IConfig>()->log_path());
    if (!logger) return unexpected("Logger: " + logger.error());
    services.register_service<ILogger>(std::move(*logger));
    
    auto db = Database::connect(services.require<IConfig>()->db_url());
    if (!db) return unexpected("Database: " + db.error());
    services.register_service<IDatabase>(std::move(*db));
    
    return services;
}
```

The initialization order is now visible in the code: config first, then logger (which needs config), then database (which needs config).

### Example 2: Testing with Mocks

Functions that access globals directly can't be tested in isolation. You can't verify that `send_notification` sends an email without actually configuring a mailer, database, and logger:

```cpp
void send_notification(int user_id, const char* msg) {
    User* user = g_db->find_user(user_id);  // Global database
    if (user && user->email) {
        g_mailer->send(user->email, msg);   // Global mailer
    }
    g_logger->log("Notification sent");     // Global logger
}

// Test is impossible without modifying globals
```

With ServiceLocator, you create a test locator with mock implementations. The function behavior is now fully testable:

```cpp
void send_notification(ServiceLocator& services, int user_id, std::string_view msg) {
    auto db = services.require<IDatabase>();
    auto mailer = services.require<IMailer>();
    auto logger = services.require<ILogger>();
    
    if (auto user = db->find_user(user_id); user && user->email) {
        mailer->send(user->email, msg);
    }
    logger->log("Notification sent");
}

TEST_CASE(notification_sends_email_to_valid_user) {
    ServiceLocator services;
    
    auto mock_db = std::make_shared<MockDatabase>();
    mock_db->add_user(User{.id = 1, .email = "test@example.com"});
    
    auto mock_mailer = std::make_shared<MockMailer>();
    auto mock_logger = std::make_shared<MockLogger>();
    
    services.register_service<IDatabase>(mock_db);
    services.register_service<IMailer>(mock_mailer);
    services.register_service<ILogger>(mock_logger);
    
    send_notification(services, 1, "Hello");
    
    ASSERT_EQ(mock_mailer->sent_count(), 1);
    ASSERT_EQ(mock_mailer->last_recipient(), "test@example.com");
}
```

### Example 3: Plugin System

Plugins that mutate global state affect the entire process. Once a plugin installs its custom filesystem, all code—including other plugins—uses it:

```cpp
// Plugin changes global state
void plugin_init() {
    g_filesystem = &custom_fs;     // Affects entire process!
    g_allocator = &custom_alloc;   // Can't undo this
}
```

With ServiceLocator hierarchies, plugins get their own scoped services that don't affect the host application:

```cpp
class Plugin {
    ServiceLocator mLocalServices;
public:
    Plugin(ServiceLocator& parent) {
        // Inherit parent services
        mLocalServices = parent.create_child();
        
        // Override specific services for this plugin
        mLocalServices.register_service<IFileSystem>(
            std::make_shared<SandboxedFS>("/plugin/data"));
    }
    
    void run() {
        // Uses sandboxed filesystem
        auto fs = mLocalServices.require<IFileSystem>();
        fs->write("data.txt", content);  // Writes to /plugin/data/data.txt
    }
};
```

The plugin's filesystem is sandboxed. Other plugins and the host application continue using the original filesystem.

---

## Advanced Patterns

Once you've migrated basic services, these patterns address more sophisticated scenarios.

### Pattern: Lazy Service Initialization

Some services are expensive to create and may not be needed in every code path. Register a factory instead of an instance. The service is created on first access:

```cpp
services.register_factory<IDatabase>([]() {
    return std::make_shared<PostgresDB>(get_connection_string());
});

// Database created on first get<IDatabase>() call
```

The factory captures any context it needs. The service is created lazily and then cached—subsequent calls return the same instance.

### Pattern: Service Hierarchies

Web servers often need per-request services that inherit from global services. A child locator sees its parent's services but can override specific ones:

```cpp
ServiceLocator global_services;
global_services.register_service<ILogger>(file_logger);

// Per-request services inherit from global
void handle_request(const Request& req) {
    auto request_services = global_services.create_child();
    request_services.register_service<IRequestContext>(
        std::make_shared<RequestContext>(req));
    
    // Can access both global (ILogger) and request-local (IRequestContext)
    process(request_services);
}
```

The request handler gets the global logger but has its own request context. When the request completes, the child locator is destroyed along with the request-scoped services.

### Pattern: Service Decoration

Add cross-cutting concerns like logging or metrics by wrapping existing services. The decorated service replaces the original:

```cpp
auto base_logger = services.get<ILogger>();
auto metrics_logger = std::make_shared<MetricsLogger>(base_logger);
services.register_service<ILogger>(metrics_logger);  // Replaces original
```

Now all logging goes through the metrics wrapper, which can count log calls, measure latency, or add context before delegating to the base logger.

### Pattern: Conditional Services

Configuration can drive which implementations are registered. In development, use mock payment processing; in production, use the real thing:

```cpp
void setup_services(ServiceLocator& services, const Config& config) {
    if (config.use_mock_payments) {
        services.register_service<IPaymentProcessor>(
            std::make_shared<MockPaymentProcessor>());
    } else {
        services.register_service<IPaymentProcessor>(
            std::make_shared<StripeProcessor>(config.stripe_key));
    }
}
```

The rest of the code doesn't know or care which implementation is registered. It just asks for an IPaymentProcessor and uses it.

---

## Verification

ServiceLocator provides both compile-time and runtime guarantees.

### Compile-Time Safety

Type safety is enforced by the template system. You get back a `shared_ptr<T>` for whatever type T you request. Calling methods that don't exist on the interface is a compile error:

```cpp
ServiceLocator services;

// Type-safe: wrong type is compile error
auto logger = services.get<ILogger>();  // Returns shared_ptr<ILogger>
// logger->query(...);  // ERROR: ILogger has no query()

// require() throws if service missing
auto db = services.require<IDatabase>();  // Throws ServiceNotFound
```

### Runtime Tests

Unit tests verify the core behaviors. Missing services return nullptr (or throw, for `require()`):

```cpp
TEST_CASE(missing_service_returns_nullptr) {
    ServiceLocator services;
    auto logger = services.get<ILogger>();
    ASSERT_TRUE(logger == nullptr);
}

TEST_CASE(require_throws_for_missing_service) {
    ServiceLocator services;
    ASSERT_THROWS(services.require<ILogger>(), ServiceNotFound);
}
```

Scoped overrides should restore the original service when the guard goes out of scope:

```cpp
TEST_CASE(scoped_override_restores_original) {
    ServiceLocator services;
    auto original = std::make_shared<FileLogger>();
    services.register_service<ILogger>(original);
    
    {
        auto mock = std::make_shared<MockLogger>();
        auto override = services.override_service<ILogger>(mock);
        
        ASSERT_EQ(services.get<ILogger>().get(), mock.get());
    }
    
    ASSERT_EQ(services.get<ILogger>().get(), original.get());
}
```

Services should be destroyed in reverse registration order, so services can use their dependencies in their destructors:

```cpp
TEST_CASE(services_destroyed_in_reverse_order) {
    std::vector<int> destruction_order;
    
    {
        ServiceLocator services;
        services.register_service<First>(
            std::make_shared<TrackedService>(destruction_order, 1));
        services.register_service<Second>(
            std::make_shared<TrackedService>(destruction_order, 2));
    }
    
    ASSERT_EQ(destruction_order, (std::vector<int>{2, 1}));
}
```

---

## When ServiceLocator Loses

ServiceLocator adds indirection. For most code, the benefits—testability, explicit dependencies, controlled initialization—outweigh the costs. But some scenarios call for different approaches.

### 1. Genuine Global Singletons

Some resources are inherently process-global. Signal handlers, atexit callbacks, and process-wide configuration don't benefit from ServiceLocator:

```cpp
// These are inherently global - ServiceLocator adds no value
std::signal(SIGTERM, handle_term);
std::atexit(cleanup);
```

You can't have different signal handlers for different "contexts" in a process. The OS only sees one handler per signal.

### 2. Performance-Critical Hot Paths

Service lookup takes ~5-10ns—negligible for most code, but it adds up in tight loops. Cache the service reference outside the loop:

```cpp
// Cache the service reference outside the loop
auto logger = services.require<ILogger>();
for (size_t i = 0; i < 1000000; ++i) {
    logger->log(data[i]);  // Use cached reference
}
```

The lookup happens once; the million iterations use the cached pointer.

### 3. Simple Applications

For small programs with few dependencies, ServiceLocator is overkill. Just pass dependencies directly:

```cpp
// Overkill for simple CLI tool
int main(int argc, char** argv) {
    Logger logger;           // Just use locals
    process(argc, argv, logger);
}
```

If you have three services and ten functions, direct parameter passing is simpler than setting up a locator.

### 4. Cross-DSO Type Identity

Dynamic libraries have separate RTTI. The same `ILogger` type in the host and plugin may have different `type_info` addresses. Use `TypeNamePolicy` for cross-DSO service lookup:

```cpp
// For plugin systems crossing DSO boundaries
using PluginLocator = ServiceLocator<TypeNamePolicy>;
```

This uses type names instead of type_info pointers, which work across DSO boundaries.

### 5. Complex Dependency Graphs

ServiceLocator doesn't handle circular dependencies. If A depends on B, B depends on C, and C depends on A, you need a full dependency injection framework that can construct the graph:

```cpp
// ServiceLocator doesn't handle:
// A depends on B, B depends on C, C depends on A
// Use a full DI framework for this
```

Libraries like Boost.DI or Google Fruit can resolve circular dependencies through lazy initialization or setter injection.

---

## Summary

Global state is seductive because it's easy. Every function can reach out and grab what it needs without ceremony. But that ease comes at a cost: hidden dependencies that make code untestable, initialization order bugs that appear and disappear based on link order, and thread safety issues that only manifest under load.

ServiceLocator makes dependencies explicit. Function signatures tell the truth about what they need. Initialization order is visible in your main function. Tests can substitute mock implementations through the same interface production code uses. The ~5-10ns lookup overhead is negligible for anything except the tightest inner loops.

| Aspect | Global Pattern | ServiceLocator |
|--------|---------------|----------------|
| Dependencies | Hidden in implementation | Explicit in function signatures |
| Initialization order | Undefined across TUs | Explicit registration order |
| Test isolation | Impossible without mutation | Scoped overrides |
| Thread safety | Manual, error-prone | Built-in synchronization |
| Lifetime management | Undefined destruction order | Reverse registration order |
| Runtime overhead | 0 (direct access) | ~5-10ns (hash lookup) |

The migration pays off immediately when your code becomes testable. In the short term, you eliminate the static initialization order fiasco. In the long term, explicit dependencies make the codebase easier to understand, refactor, and maintain.

---

## References

- [Dependency Injection Principles, Practices, and Patterns](https://www.manning.com/books/dependency-injection-principles-practices-patterns) — Steven van Deursen
- [Martin Fowler: Inversion of Control Containers](https://martinfowler.com/articles/injection.html)
- Fat-P User Manual: ServiceLocator — Complete API reference

---

*FAT-P Library Documentation — January 2025*
