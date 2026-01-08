---
doc_id: MG-FACTORY-001
doc_type: "Migration Guide"
title: "Type Switches to Registered Factories"
from_pattern: "switch on type tag, function pointer tables, manual dispatch"
to_component: "Factory"
fatp_version: "1.0"
cxx_standard: "C++17"
migration_complexity: "Medium"
breaking_changes: true
last_verified: "2025-01-08"
---

# Migration Guide - Type Switches to Registered Factories

### *From `switch(type)` to Extensible Factory Registration*

*FAT-P Library — January 2025*

---

## Migration Card

| Aspect | Detail |
|--------|--------|
| **C Pattern** | `switch` on type enum, function pointer tables, manual constructor dispatch |
| **Problems Solved** | Enum/switch mismatch, closed for extension, scattered creation, no error handling |
| **Fat-P Component** | `Factory<Key, Product, Policies...>` |
| **Migration Complexity** | Medium — requires restructuring creation logic |
| **Runtime Overhead** | O(log n) or O(1) lookup depending on storage policy |
| **Breaking Changes** | Yes — from static dispatch to dynamic registration |

---

## Table of Contents

1. [The Problem with Type Switches](#the-problem-with-type-switches)
2. [Real-World Factory Disasters](#real-world-factory-disasters)
3. [The C Patterns](#the-c-patterns)
4. [The Factory Solution](#the-factory-solution)
5. [Migration Steps](#migration-steps)
6. [Before/After Examples](#beforeafter-examples)
7. [Advanced Patterns](#advanced-patterns)
8. [Verification](#verification)
9. [When Factory Loses](#when-factory-loses)

---

## The Problem with Type Switches

Object creation by type is fundamental to software systems: parsers, serializers, plugin systems, game engines. The naive C/C++ approach:

```cpp
enum class WidgetType { Button, Label, TextBox, Checkbox };

std::unique_ptr<Widget> createWidget(WidgetType type) {
    switch (type) {
        case WidgetType::Button:   return std::make_unique<Button>();
        case WidgetType::Label:    return std::make_unique<Label>();
        case WidgetType::TextBox:  return std::make_unique<TextBox>();
        case WidgetType::Checkbox: return std::make_unique<Checkbox>();
        // Added WidgetType::Slider to enum...
        // ... forgot to add case here
    }
    return nullptr;  // Silent failure
}
```

**The problems accumulate:**

| Problem | Consequence |
|---------|-------------|
| Missing case | Compiler warning (maybe), runtime null |
| Adding types | Must modify enum AND switch (two places) |
| Plugin types | Impossible—switch is compile-time |
| Error handling | Inconsistent (null? throw? log?) |
| Testing | Must mock entire factory |
| Statistics | Manual instrumentation |

---

## Real-World Factory Disasters

### SQLite's VFS Registration

SQLite needs to create different Virtual File System implementations at runtime. From [`src/main.c`](https://github.com/sqlite/sqlite/blob/master/src/main.c):

```c
/*
** Locate and return a pointer to the VFS function for the given name.
** If no match is found, return NULL.
*/
sqlite3_vfs *sqlite3_vfs_find(const char *zVfs){
  sqlite3_vfs *pVfs = 0;
  sqlite3_mutex *mutex = sqlite3MutexAlloc(SQLITE_MUTEX_STATIC_MAIN);
  sqlite3_mutex_enter(mutex);
  for(pVfs=vfsList; pVfs; pVfs=pVfs->pNext){
    if( zVfs==0 ) break;
    if( strcmp(zVfs, pVfs->zName)==0 ) break;
  }
  sqlite3_mutex_leave(mutex);
  return pVfs;
}
```

And registration:

```c
int sqlite3_vfs_register(sqlite3_vfs *pVfs, int makeDflt){
  sqlite3_mutex *mutex = sqlite3MutexAlloc(SQLITE_MUTEX_STATIC_MAIN);
  sqlite3_mutex_enter(mutex);
  vfsUnlink(pVfs);
  if( makeDflt || vfsList==0 ){
    pVfs->pNext = vfsList;
    vfsList = pVfs;
  }else{
    pVfs->pNext = vfsList->pNext;
    vfsList->pNext = pVfs;
  }
  sqlite3_mutex_leave(mutex);
  return SQLITE_OK;
}
```

**This is a factory pattern**—but implemented with manual linked lists, explicit locking, and string comparisons. Fat-P's Factory provides this with type safety and policy customization.

### The Plugin System Switch

```cpp
// plugins.cpp - central knowledge of all plugins
enum PluginType { 
    PLUGIN_JPEG, 
    PLUGIN_PNG, 
    PLUGIN_GIF,
    // Every new format requires editing this enum
};

std::unique_ptr<ImagePlugin> loadPlugin(PluginType type) {
    switch (type) {
        case PLUGIN_JPEG: return std::make_unique<JpegPlugin>();
        case PLUGIN_PNG:  return std::make_unique<PngPlugin>();
        case PLUGIN_GIF:  return std::make_unique<GifPlugin>();
    }
    return nullptr;
}

// Loading from user input
PluginType typeFromString(const std::string& name) {
    if (name == "jpeg") return PLUGIN_JPEG;
    if (name == "png")  return PLUGIN_PNG;
    if (name == "gif")  return PLUGIN_GIF;
    // ANOTHER switch that must stay in sync!
    throw std::runtime_error("Unknown plugin: " + name);
}
```

**Problems:**
- Adding WebP requires modifying: enum, switch, string mapper
- Can't load plugins at runtime
- Third-party plugins impossible without recompilation

---

## The C Patterns

### Pattern 1: Switch on Enum

```cpp
enum ShapeType { CIRCLE, RECTANGLE, TRIANGLE };

Shape* createShape(ShapeType type) {
    switch (type) {
        case CIRCLE:    return new Circle();
        case RECTANGLE: return new Rectangle();
        case TRIANGLE:  return new Triangle();
        default:        return nullptr;  // Silent failure
    }
}
```

**Problems:**
- Must modify two places to add type
- No extensibility
- Default case hides bugs

### Pattern 2: Function Pointer Table

```c
typedef Shape* (*ShapeCreator)(void);

struct ShapeFactory {
    const char* name;
    ShapeCreator create;
};

static ShapeFactory factories[] = {
    {"circle", create_circle},
    {"rectangle", create_rectangle},
    {"triangle", create_triangle},
    {NULL, NULL}  // Sentinel
};

Shape* createShape(const char* name) {
    for (int i = 0; factories[i].name; i++) {
        if (strcmp(name, factories[i].name) == 0) {
            return factories[i].create();
        }
    }
    return NULL;
}
```

**Problems:**
- O(n) lookup
- Static array—can't add at runtime
- No error handling
- No thread safety

### Pattern 3: Map of Creators

```cpp
std::map<std::string, std::function<std::unique_ptr<Shape>()>> factories;

void registerFactories() {
    factories["circle"] = []{ return std::make_unique<Circle>(); };
    factories["rectangle"] = []{ return std::make_unique<Rectangle>(); };
}

std::unique_ptr<Shape> createShape(const std::string& name) {
    auto it = factories.find(name);
    if (it == factories.end()) {
        return nullptr;  // Or throw? Inconsistent.
    }
    return it->second();
}
```

**Problems:**
- Global mutable state
- Registration order matters
- No statistics
- No duplicate checking
- Thread safety is manual

### Pattern 4: SQLite-Style Registration

```c
/* Linked list of registered types */
struct TypeRegistry {
    const char* name;
    void* (*create)(void* arg);
    struct TypeRegistry* next;
};

static TypeRegistry* registry = NULL;
static pthread_mutex_t registry_mutex = PTHREAD_MUTEX_INITIALIZER;

int register_type(const char* name, void* (*create)(void*)) {
    TypeRegistry* entry = malloc(sizeof(TypeRegistry));
    entry->name = strdup(name);
    entry->create = create;
    
    pthread_mutex_lock(&registry_mutex);
    entry->next = registry;
    registry = entry;
    pthread_mutex_unlock(&registry_mutex);
    
    return 0;
}

void* create_type(const char* name, void* arg) {
    pthread_mutex_lock(&registry_mutex);
    for (TypeRegistry* r = registry; r; r = r->next) {
        if (strcmp(r->name, name) == 0) {
            pthread_mutex_unlock(&registry_mutex);
            return r->create(arg);  // DANGER: calling create outside lock
        }
    }
    pthread_mutex_unlock(&registry_mutex);
    return NULL;
}
```

**Problems:**
- Manual memory management
- O(n) lookup
- Creator called outside lock (what if unregistered?)
- No error details
- Memory leaks on shutdown

---

## The Factory Solution

### Core Concept

`Factory` provides type-safe, policy-based object creation with registration:

```cpp
#include "Factory.h"
using namespace fat_p;

// Define factory type
using ShapeFactory = Factory<
    std::string,                  // Key type
    std::unique_ptr<Shape>,       // Product type
    SharedMutexPolicy             // Thread-safe
>;

// Create factory instance
ShapeFactory factory;

// Register creators
factory.registerType("circle", []{ return std::make_unique<Circle>(); });
factory.registerType("rectangle", []{ return std::make_unique<Rectangle>(); });

// Create objects
auto result = factory.make("circle");
if (result) {
    auto shape = std::move(*result);
    shape->draw();
} else {
    std::cerr << result.error().full_message() << "\n";
}
```

### Key Features

| Feature | Benefit |
|---------|---------|
| **Type-safe keys** | String, int, enum—any hashable type |
| **Expected return** | Clear error handling, no null surprises |
| **Policy-based** | Customize threading, storage, errors, stats |
| **Runtime registration** | Add types without recompilation |
| **Duplicate prevention** | `PreventOverwritePolicy` rejects re-registration |
| **Statistics** | Track registrations, lookups, failures |
| **Thread-safe option** | `SharedMutexPolicy` for concurrent access |

### Policy System

```cpp
template <
    typename K,                           // Key type
    typename T,                           // Product type
    typename ConcurrencyPolicy,           // Threading
    typename ErrorHandlingPolicy,         // How to handle errors
    typename RegistrationPolicy,          // Allow overwrite?
    typename StoragePolicy,               // Map or unordered_map
    typename LifetimePolicy,              // Instance or singleton
    typename StatisticsPolicy,            // Track stats?
    typename... Params                    // Creator parameters
>
class Factory;
```

### Pre-Built Aliases

```cpp
// Simple single-threaded factory
template<typename K, typename T>
using SimpleFactory = Factory<K, T, SingleThreadedPolicy, ...>;

// Thread-safe factory
template<typename K, typename T>
using ThreadSafeFactory = Factory<K, T, MutexSynchronizationPolicy, ...>;

// Fast string-keyed factory (unordered_map)
template<typename T>
using StringKeyFactory = FastFactory<std::string, T>;

// HPC factory with zero statistics overhead
template<typename K, typename T>
using HPCFactory = Factory<K, T, ..., NoStatisticsPolicy>;
```

### API Overview

```cpp
template <typename K, typename T, typename... Policies>
class Factory {
public:
    // Registration
    template<typename Callable>
    [[nodiscard]] bool registerType(const K& key, Callable&& creator);
    
    size_t registerTypes(std::initializer_list<std::pair<K, Creator>> list);
    
    [[nodiscard]] bool unregisterType(const K& key);
    
    // Creation
    [[nodiscard]] Expected<T, ErrorInfo> make(const K& key, Params... params) const;
    
    // Query
    [[nodiscard]] bool hasType(const K& key) const noexcept;
    [[nodiscard]] size_t size() const noexcept;
    [[nodiscard]] std::vector<K> getRegisteredKeys() const;
    
    // Statistics
    [[nodiscard]] Stats::Snapshot getStats() const noexcept;
    void resetStats() noexcept;
    void clear() noexcept;
};
```

---

## Migration Steps

### Step 1: Identify Factory Patterns

Find creation switches and tables:

```bash
grep -rn "switch.*Type\|switch.*Kind\|switch.*Mode" src/
grep -rn "create.*switch\|new.*case" src/
grep -rn "std::map.*function\|std::map.*Creator" src/
```

### Step 2: Define Product Interface

Ensure all created types share a common interface:

```cpp
// Before: no common base
class Circle { void draw(); };
class Rectangle { void draw(); };

// After: shared interface
class Shape {
public:
    virtual ~Shape() = default;
    virtual void draw() = 0;
};

class Circle : public Shape { void draw() override; };
class Rectangle : public Shape { void draw() override; };
```

### Step 3: Choose Key Type

```cpp
// String keys (most flexible)
using ShapeFactory = Factory<std::string, std::unique_ptr<Shape>>;

// Enum keys (type-safe, fast)
enum class ShapeType { Circle, Rectangle, Triangle };
using ShapeFactory = Factory<ShapeType, std::unique_ptr<Shape>>;

// Integer keys (e.g., type codes from file format)
using ShapeFactory = Factory<uint32_t, std::unique_ptr<Shape>>;
```

### Step 4: Replace Switch with Registration

**Before:**
```cpp
std::unique_ptr<Shape> createShape(const std::string& type) {
    if (type == "circle")    return std::make_unique<Circle>();
    if (type == "rectangle") return std::make_unique<Rectangle>();
    if (type == "triangle")  return std::make_unique<Triangle>();
    throw std::runtime_error("Unknown shape: " + type);
}
```

**After:**
```cpp
// In initialization code (e.g., main or module init)
shapeFactory.registerType("circle", []{ 
    return std::make_unique<Circle>(); 
});
shapeFactory.registerType("rectangle", []{ 
    return std::make_unique<Rectangle>(); 
});
shapeFactory.registerType("triangle", []{ 
    return std::make_unique<Triangle>(); 
});

// In creation code
auto result = shapeFactory.make(type);
if (!result) {
    throw std::runtime_error(result.error().full_message());
}
return std::move(*result);
```

### Step 5: Handle Errors Properly

```cpp
// Use Expected for clean error handling
auto result = factory.make("unknown");
if (!result) {
    switch (result.error().code) {
        case FactoryError::KeyNotFound:
            log_warning("Shape type not registered: {}", type);
            return createDefaultShape();
        case FactoryError::CreationFailed:
            log_error("Failed to create shape: {}", result.error().message);
            throw;
        default:
            throw std::runtime_error(result.error().full_message());
    }
}
```

### Step 6: Add Plugin Support (Optional)

```cpp
// Plugin registration API
extern "C" void register_shapes(ShapeFactory& factory) {
    factory.registerType("star", []{ return std::make_unique<Star>(); });
    factory.registerType("hexagon", []{ return std::make_unique<Hexagon>(); });
}

// Dynamic loading
void loadPlugins(ShapeFactory& factory) {
    for (auto& path : getPluginPaths()) {
        void* handle = dlopen(path.c_str(), RTLD_NOW);
        auto register_fn = (void(*)(ShapeFactory&))dlsym(handle, "register_shapes");
        if (register_fn) {
            register_fn(factory);
        }
    }
}
```

---

## Before/After Examples

### Example 1: Document Parser Factory

**Before (switch):**
```cpp
enum class DocType { PDF, DOCX, TXT, HTML, UNKNOWN };

DocType detectType(const std::string& path) {
    if (endsWith(path, ".pdf"))  return DocType::PDF;
    if (endsWith(path, ".docx")) return DocType::DOCX;
    if (endsWith(path, ".txt"))  return DocType::TXT;
    if (endsWith(path, ".html")) return DocType::HTML;
    return DocType::UNKNOWN;
}

std::unique_ptr<Parser> createParser(DocType type) {
    switch (type) {
        case DocType::PDF:  return std::make_unique<PdfParser>();
        case DocType::DOCX: return std::make_unique<DocxParser>();
        case DocType::TXT:  return std::make_unique<TxtParser>();
        case DocType::HTML: return std::make_unique<HtmlParser>();
        default: return nullptr;  // UNKNOWN falls through
    }
}

// Adding new format requires:
// 1. Add to enum
// 2. Add to detectType
// 3. Add to switch
// 4. Recompile everything
```

**After (Factory):**
```cpp
using ParserFactory = StringKeyFactory<std::unique_ptr<Parser>>;
ParserFactory parserFactory;

// Registration (can be in separate modules)
void initParsers() {
    parserFactory.registerType(".pdf", []{ return std::make_unique<PdfParser>(); });
    parserFactory.registerType(".docx", []{ return std::make_unique<DocxParser>(); });
    parserFactory.registerType(".txt", []{ return std::make_unique<TxtParser>(); });
    parserFactory.registerType(".html", []{ return std::make_unique<HtmlParser>(); });
}

// Usage
std::unique_ptr<Parser> createParser(const std::string& path) {
    std::string ext = getExtension(path);
    auto result = parserFactory.make(ext);
    if (!result) {
        throw std::runtime_error("No parser for: " + ext);
    }
    return std::move(*result);
}

// Adding new format:
// Just call parserFactory.registerType(".md", [...]);
// No recompilation of existing code!
```

### Example 2: Game Entity Factory

**Before (function pointers):**
```c
typedef Entity* (*EntityCreator)(float x, float y);

struct EntityType {
    const char* name;
    EntityCreator create;
};

static EntityType entity_types[] = {
    {"player", create_player},
    {"enemy", create_enemy},
    {"bullet", create_bullet},
    {NULL, NULL}
};

Entity* spawn_entity(const char* type, float x, float y) {
    for (int i = 0; entity_types[i].name; i++) {
        if (strcmp(type, entity_types[i].name) == 0) {
            return entity_types[i].create(x, y);
        }
    }
    fprintf(stderr, "Unknown entity: %s\n", type);
    return NULL;
}
```

**After (Factory with parameters):**
```cpp
// Factory with creation parameters
using EntityFactory = Factory<
    std::string,
    std::unique_ptr<Entity>,
    SingleThreadedPolicy,
    factory::ExpectedErrorPolicy<std::unique_ptr<Entity>, std::string>,
    PreventOverwritePolicy,
    UnorderedMapStoragePolicy<std::string, std::function<std::unique_ptr<Entity>(float, float)>>,
    InstanceLifetimePolicy,
    AtomicStatisticsPolicy,
    float, float  // Parameters: x, y
>;

EntityFactory entityFactory;

void initEntities() {
    entityFactory.registerType("player", [](float x, float y) { 
        return std::make_unique<Player>(x, y); 
    });
    entityFactory.registerType("enemy", [](float x, float y) { 
        return std::make_unique<Enemy>(x, y); 
    });
    entityFactory.registerType("bullet", [](float x, float y) { 
        return std::make_unique<Bullet>(x, y); 
    });
}

std::unique_ptr<Entity> spawnEntity(const std::string& type, float x, float y) {
    auto result = entityFactory.make(type, x, y);
    if (!result) {
        log_error("Failed to spawn {}: {}", type, result.error().message);
        return nullptr;
    }
    return std::move(*result);
}
```

### Example 3: Serializer Registry

**Before (global map):**
```cpp
std::unordered_map<std::string, std::function<void(std::ostream&, const Object&)>> serializers;
std::mutex serializer_mutex;

void registerSerializer(const std::string& format, 
                        std::function<void(std::ostream&, const Object&)> fn) {
    std::lock_guard lock(serializer_mutex);
    serializers[format] = fn;  // Silently overwrites!
}

void serialize(const std::string& format, std::ostream& out, const Object& obj) {
    std::lock_guard lock(serializer_mutex);
    auto it = serializers.find(format);
    if (it == serializers.end()) {
        throw std::runtime_error("No serializer for: " + format);
    }
    it->second(out, obj);  // Called inside lock - potential deadlock!
}
```

**After (Factory with proper policies):**
```cpp
// Serializer function type
using Serializer = std::function<void(std::ostream&, const Object&)>;

// Thread-safe factory that prevents overwrite
using SerializerFactory = Factory<
    std::string,
    Serializer,
    SharedMutexPolicy,         // Thread-safe with reader preference
    factory::ThrowingErrorPolicy<Serializer, std::string>,
    PreventOverwritePolicy     // No silent overwrites
>;

SerializerFactory serializerFactory;

bool registerSerializer(const std::string& format, Serializer fn) {
    return serializerFactory.registerType(format, [fn]{ return fn; });
}

void serialize(const std::string& format, std::ostream& out, const Object& obj) {
    auto result = serializerFactory.make(format);
    // Creator called OUTSIDE lock - no deadlock risk
    (*result)(out, obj);
}
```

---

## Advanced Patterns

### Pattern: Self-Registering Types

```cpp
// Base class with registration macro
#define REGISTER_SHAPE(ClassName, KeyName) \
    static bool _registered_##ClassName = []{ \
        ShapeFactory::instance().registerType(KeyName, \
            []{ return std::make_unique<ClassName>(); }); \
        return true; \
    }()

class Circle : public Shape {
public:
    void draw() override { /* ... */ }
};
REGISTER_SHAPE(Circle, "circle");

class Rectangle : public Shape {
public:
    void draw() override { /* ... */ }
};
REGISTER_SHAPE(Rectangle, "rectangle");
```

### Pattern: Factory with Validation

```cpp
using ValidatedFactory = Factory<
    std::string,
    std::unique_ptr<Config>,
    SingleThreadedPolicy,
    factory::ExpectedErrorPolicy<std::unique_ptr<Config>, std::string>
>;

configFactory.registerType("database", []() -> std::unique_ptr<Config> {
    auto config = std::make_unique<DatabaseConfig>();
    if (!config->validate()) {
        throw std::runtime_error("Invalid database configuration");
    }
    return config;
});

// Factory catches exception and returns error
auto result = configFactory.make("database");
if (!result) {
    if (result.error().code == FactoryError::CreationFailed) {
        // Handle validation failure
    }
}
```

### Pattern: Statistics Monitoring

```cpp
void monitorFactory() {
    auto stats = shapeFactory.getStats();
    
    metrics.counter("factory.registrations", stats.registrations);
    metrics.counter("factory.lookups", stats.lookups);
    metrics.counter("factory.resolutions", stats.resolutions);
    metrics.counter("factory.failures", stats.resolution_failures);
    
    // Calculate hit rate
    double hitRate = stats.lookups > 0 
        ? (double)stats.resolutions / stats.lookups 
        : 1.0;
    metrics.gauge("factory.hit_rate", hitRate);
}
```

### Pattern: Factory with Fallback

```cpp
std::unique_ptr<Shape> createWithFallback(const std::string& type) {
    auto result = shapeFactory.make(type);
    if (result) {
        return std::move(*result);
    }
    
    // Log and return default
    log_warning("Unknown shape '{}', using default", type);
    return shapeFactory.make("default").value();  // Assumes default exists
}
```

---

## Verification

### Compile-Time Verification

```cpp
// [[nodiscard]] prevents ignoring registration result
factory.registerType("x", creator);  // Warning: ignoring return value

// Type safety on keys
Factory<int, Shape*> intFactory;
intFactory.make("string");  // Compile error: string != int
```

### Runtime Verification

```cpp
TEST(Factory, BasicRegistrationAndCreation) {
    SimpleFactory<std::string, int> factory;
    
    EXPECT_TRUE(factory.registerType("answer", []{ return 42; }));
    EXPECT_TRUE(factory.hasType("answer"));
    
    auto result = factory.make("answer");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 42);
}

TEST(Factory, PreventsDuplicateRegistration) {
    SimpleFactory<std::string, int> factory;
    
    EXPECT_TRUE(factory.registerType("x", []{ return 1; }));
    EXPECT_FALSE(factory.registerType("x", []{ return 2; }));  // Rejected
    
    EXPECT_EQ(*factory.make("x"), 1);  // Original still works
}

TEST(Factory, HandlesUnknownKey) {
    SimpleFactory<std::string, int> factory;
    
    auto result = factory.make("unknown");
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, FactoryError::KeyNotFound);
}

TEST(Factory, Statistics) {
    SimpleFactory<std::string, int> factory;
    
    factory.registerType("a", []{ return 1; });
    factory.make("a");
    factory.make("a");
    factory.make("b");  // Fails
    
    auto stats = factory.getStats();
    EXPECT_EQ(stats.registrations, 1);
    EXPECT_EQ(stats.lookups, 3);
    EXPECT_EQ(stats.resolutions, 2);
    EXPECT_EQ(stats.resolution_failures, 1);
}

TEST(Factory, ThreadSafety) {
    ThreadSafeFactory<int, int> factory;
    std::atomic<int> successCount{0};
    
    // Register from multiple threads
    std::vector<std::thread> threads;
    for (int i = 0; i < 100; i++) {
        threads.emplace_back([&factory, i, &successCount] {
            if (factory.registerType(i, [i]{ return i * 2; })) {
                successCount++;
            }
        });
    }
    for (auto& t : threads) t.join();
    
    EXPECT_EQ(successCount.load(), 100);
    EXPECT_EQ(factory.size(), 100);
}
```

---

## When Factory Loses

### 1. Compile-Time Known Types

If all types are known at compile time and performance is critical:

```cpp
// Switch is faster - no map lookup
switch (type) {
    case Type::A: return A();
    case Type::B: return B();
}
```

Factory adds overhead (~50ns) for the lookup and std::function call.

### 2. Very Few Types

With 2-3 types, switch is simpler:

```cpp
// Simpler than factory for 2 types
if (type == "json") return parseJson(data);
if (type == "xml")  return parseXml(data);
```

### 3. Singleton Products

If you need the same instance every time:

```cpp
// Factory creates new instances
auto a = factory.make("config");
auto b = factory.make("config");
// a != b - different instances

// For singletons, use ServiceLocator instead
```

### 4. Complex Creation Parameters

If creation requires many context-dependent parameters:

```cpp
// Awkward with factory
factory.registerType("widget", [](int x, int y, int w, int h, 
                                   Color bg, Font f, ...) { ... });

// Direct construction may be clearer
auto widget = std::make_unique<Widget>(params...);
```

---

## Summary

| Aspect | Switch Pattern | Factory |
|--------|---------------|---------|
| Adding types | Modify enum + switch | Just register |
| Plugin types | Impossible | Runtime registration |
| Error handling | Inconsistent | Expected<T,E> |
| Thread safety | Manual | Policy-based |
| Statistics | Manual | Built-in |
| Duplicate keys | Silent overwrite | Policy-controlled |
| Type safety | Limited | Full |
| Runtime cost | O(1) jump | O(log n) or O(1) lookup |

**Migration ROI:**
- **Immediate:** Consistent error handling, no missing cases
- **Short-term:** Plugin support, runtime extensibility
- **Long-term:** Statistics, monitoring, testability

---

## References

- [SQLite VFS Registration](https://github.com/sqlite/sqlite/blob/master/src/main.c) — C-style factory pattern
- Gang of Four: Factory Method and Abstract Factory patterns
- Fat-P User Manual: Factory — Complete API reference
- Fat-P User Manual: Expected — Error handling

---

*FAT-P Library Documentation — January 2025*
