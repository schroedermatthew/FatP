---
doc_id: MG-SIGNAL-001
doc_type: "Migration Guide"
title: "Callback Functions to Type-Safe Observers"
from_pattern: "Function pointers, callback registration, observer lists"
to_component: "Signal"
fatp_version: "1.0"
cxx_standard: "C++17"
migration_complexity: "Medium"
breaking_changes: true
last_verified: "2025-01-08"
---

# Migration Guide - Callback Functions to Type-Safe Observers

### *From `void (*callback)(void*)` to `Signal<void(Args...)>`*

*FAT-P Library — January 2025*

---

## Migration Card

| Aspect | Detail |
|--------|--------|
| **C Pattern** | Function pointers, callback + context pairs, manual observer lists |
| **Problems Solved** | Dangling callbacks, type erasure bugs, manual lifetime, no disconnect |
| **Fat-P Component** | `Signal<Signature, SyncPolicy, EmissionPolicy, InlineCapacity>` |
| **Migration Complexity** | Medium — requires rethinking callback ownership |
| **Runtime Overhead** | Minimal — zero heap allocation for ≤4 listeners |
| **Breaking Changes** | Yes — API redesign, but cleaner and safer |

---

## Table of Contents

1. [The Problem with C-Style Callbacks](#the-problem-with-c-style-callbacks)
2. [Real-World Callback Disasters](#real-world-callback-disasters)
3. [The C Patterns](#the-c-patterns)
4. [The Signal Solution](#the-signal-solution)
5. [Migration Steps](#migration-steps)
6. [Before/After Examples](#beforeafter-examples)
7. [Advanced Patterns](#advanced-patterns)
8. [Thread Safety](#thread-safety)
9. [Verification](#verification)
10. [When Signal Loses](#when-signal-loses)

---

## The Problem with C-Style Callbacks

Callbacks are fundamental to event-driven programming. In C, they look like this:

```c
typedef void (*EventCallback)(int event_type, void* event_data, void* user_data);

void register_callback(void* handle, EventCallback cb, void* user_data);
void unregister_callback(void* handle, EventCallback cb);
```

This pattern has served systems programming for decades. It also causes an entire category of bugs:

```c
struct Widget {
    int value;
};

void on_value_changed(int type, void* data, void* user) {
    Widget* w = (Widget*)user;  // Cast from void*
    w->value = *(int*)data;     // Another cast
}

int main() {
    Widget w;
    register_callback(handle, on_value_changed, &w);
    
    // Later: widget goes out of scope, callback still registered
}  // w destroyed, callback now points to garbage
```

**The problems:**

1. **Type erasure** — Everything is `void*`; casts can be wrong
2. **Lifetime mismatch** — Callback outlives the object it references  
3. **No automatic unregister** — Must manually track and remove callbacks
4. **Single callback limitation** — Many C APIs support only one callback
5. **Thread safety** — Usually absent or manual

---

## Real-World Callback Disasters

### SQLite Callback Patterns

SQLite uses callbacks extensively. From [`src/util.c`](https://github.com/sqlite/sqlite/blob/master/src/util.c):

```c
/* Global callback pointers */
static int (*xTestCallback)(int) = 0;
static void (*xLog)(void*, int, const char*) = 0;
static void *pLogArg = 0;
```

And from [`src/main.c`](https://github.com/sqlite/sqlite/blob/master/src/main.c):

```c
int sqlite3_exec(
    sqlite3 *db,
    const char *zSql,
    int (*xCallback)(void*,int,char**,char**),  /* Callback per row */
    void *pArg,                                   /* User context */
    char **pzErrMsg
){
    // ...
    if( xCallback && xCallback(pArg, nCol, azCols, azVals) ){
        // Callback returned non-zero: abort
    }
}
```

**Failure modes in this pattern:**

| Issue | Consequence |
|-------|-------------|
| `pArg` points to freed memory | Crash or corruption |
| Callback throws (in C++) | Undefined behavior |
| Callback modifies global state during iteration | Reentrancy bugs |
| No way to remove callback during exec | Must wait until complete |

### The Dangling Callback Problem

```cpp
class Widget {
    int mValue;
public:
    static void callback(void* user, int newValue) {
        Widget* self = static_cast<Widget*>(user);
        self->mValue = newValue;  // Crash if Widget is destroyed
    }
    
    void subscribe(EventSource* source) {
        source->setCallback(callback, this);
    }
    
    ~Widget() {
        // Forgot to unsubscribe!
        // Or: how do we even get the EventSource pointer here?
    }
};
```

This is **use-after-free** waiting to happen. The callback stores a raw pointer; when `Widget` is destroyed, the pointer dangles.

### The Reentrancy Problem

```c
struct ObserverList {
    Callback* callbacks[MAX_CALLBACKS];
    int count;
};

void notify_all(ObserverList* list, int event) {
    for (int i = 0; i < list->count; i++) {
        list->callbacks[i](event);  // What if this modifies 'list'?
    }
}

void my_callback(int event) {
    unregister_callback(global_list, my_callback);  // Modifies list during iteration!
}
```

If a callback unregisters itself (or others) during notification, the iteration corrupts.

---

## The C Patterns

### Pattern 1: Function Pointer + Context

The most common pattern:

```c
typedef void (*Callback)(void* context, int value);

struct EventSource {
    Callback callback;
    void* context;
};

void set_callback(EventSource* src, Callback cb, void* ctx) {
    src->callback = cb;
    src->context = ctx;
}

void emit(EventSource* src, int value) {
    if (src->callback) {
        src->callback(src->context, value);
    }
}
```

**Problems:**
- Only one callback per source
- No type safety on context
- Manual lifetime management
- No disconnect mechanism

### Pattern 2: Callback List with Manual Management

```c
#define MAX_CALLBACKS 16

struct CallbackEntry {
    Callback cb;
    void* context;
    int id;  // For removal
};

struct EventSource {
    CallbackEntry callbacks[MAX_CALLBACKS];
    int count;
    int next_id;
};

int add_callback(EventSource* src, Callback cb, void* ctx) {
    if (src->count >= MAX_CALLBACKS) return -1;
    int id = src->next_id++;
    src->callbacks[src->count++] = (CallbackEntry){cb, ctx, id};
    return id;
}

void remove_callback(EventSource* src, int id) {
    for (int i = 0; i < src->count; i++) {
        if (src->callbacks[i].id == id) {
            // Shift remaining callbacks
            memmove(&src->callbacks[i], &src->callbacks[i+1], 
                    (src->count - i - 1) * sizeof(CallbackEntry));
            src->count--;
            return;
        }
    }
}
```

**Problems:**
- Fixed maximum callbacks
- O(n) removal
- Not reentrant (removal during iteration corrupts)
- Manual ID tracking

### Pattern 3: Linked List of Observers

```c
struct Observer {
    void (*notify)(struct Observer*, int event);
    struct Observer* next;
};

struct Subject {
    struct Observer* observers;
};

void attach(Subject* s, Observer* o) {
    o->next = s->observers;
    s->observers = o;
}

void notify_all(Subject* s, int event) {
    Observer* o = s->observers;
    while (o) {
        Observer* next = o->next;  // Cache next before callback (might free 'o')
        o->notify(o, event);
        o = next;
    }
}
```

**Problems:**
- Observer must remain allocated
- Detach requires list traversal
- Thread safety is complex

### Pattern 4: SQLite-Style Function Table

```c
/* From SQLite: VFS callback structure */
struct sqlite3_vfs {
    int iVersion;
    int (*xOpen)(sqlite3_vfs*, const char*, sqlite3_file*, int, int*);
    int (*xDelete)(sqlite3_vfs*, const char*, int);
    int (*xAccess)(sqlite3_vfs*, const char*, int, int*);
    int (*xFullPathname)(sqlite3_vfs*, const char*, int, char*);
    /* ... many more function pointers ... */
};
```

**This pattern is appropriate for:**
- Plugin interfaces (stable ABI)
- Multiple related operations
- Infrequent registration

**Not appropriate for:**
- Many independent observers
- Frequent connect/disconnect
- Automatic lifetime management

---

## The Signal Solution

### Core Concept

`Signal` is a type-safe, multi-listener event emitter with automatic lifetime management:

```cpp
#include "Signal.h"
using namespace fat_p;

// Declare a signal with a specific signature
Signal<void(int)> onValueChanged;

// Connect listeners (returns RAII handle)
auto conn1 = onValueChanged.connect([](int v) {
    std::cout << "Value: " << v << "\n";
});

auto conn2 = onValueChanged.connect([](int v) {
    log_value(v);
});

// Emit to all listeners
onValueChanged.emit(42);
// or: onValueChanged(42);

// conn1 and conn2 automatically disconnect when destroyed
```

### Key Features

| Feature | Benefit |
|---------|---------|
| **Type-safe signature** | No `void*` casts; compiler checks argument types |
| **RAII connections** | Automatic disconnect on scope exit |
| **Multiple listeners** | Unlimited observers per signal |
| **Reentrancy-safe** | Disconnect during emit is handled correctly |
| **Small-object optimization** | No heap allocation for ≤4 listeners |
| **Thread-safe variant** | `ThreadSafeSignal` with proper locking |
| **Priority ordering** | Control callback execution order |

### API Overview

```cpp
template <typename Signature,
          typename SyncPolicy = SingleThreadedPolicy,
          typename EmissionPolicy = CatchAndIgnorePolicy,
          size_t InlineCapacity = 4>
class Signal;

// For void(int, std::string) signature:
class Signal<void(int, std::string)> {
public:
    // Connection methods
    [[nodiscard]] ScopedConnection connect(Callable&& slot);
    [[nodiscard]] ScopedConnection connect(Callable&& slot, int priority);
    [[nodiscard]] ConnectionId connectManual(Callable&& slot);
    
    // Disconnection
    bool disconnect(ConnectionId id);
    void disconnectAll();
    
    // Emission
    void emit(int arg1, std::string arg2);
    void operator()(int arg1, std::string arg2);  // Same as emit
    
    // Query
    [[nodiscard]] size_t slotCount() const;
    [[nodiscard]] size_t activeSlotCount() const;
    [[nodiscard]] bool hasConnections() const;
    [[nodiscard]] bool isConnected(ConnectionId id) const;
};

// RAII connection handle
class ScopedConnection {
public:
    void disconnect() noexcept;  // Manual disconnect
    [[nodiscard]] ConnectionId release() noexcept;  // Give up RAII
    [[nodiscard]] bool connected() const noexcept;
};
```

### ScopedConnection Lifetime

```cpp
class Widget {
    ScopedConnection mConnection;  // Member variable
    
public:
    void subscribeTo(Signal<void(int)>& signal) {
        // Store connection; auto-disconnects when Widget is destroyed
        mConnection = signal.connect([this](int v) {
            handleValue(v);
        });
    }
    
    ~Widget() {
        // mConnection destructor runs, disconnects callback automatically
        // No dangling pointer!
    }
};
```

---

## Migration Steps

### Step 1: Identify Callback Patterns

Find callback registrations in your codebase:

```bash
grep -rn "typedef.*(\*.*)(.*void\*" src/    # Function pointer typedefs
grep -rn "setCallback\|addListener\|subscribe" src/
grep -rn "void\* context\|void\* user" src/
```

Categorize by:
- Single callback vs. multi-listener
- Lifetime management approach
- Thread safety requirements

### Step 2: Define Signal Types

Replace callback typedefs with Signal declarations:

```cpp
// Before
typedef void (*ValueChangedCallback)(void* context, int oldValue, int newValue);

// After
using ValueChangedSignal = fat_p::Signal<void(int oldValue, int newValue)>;
```

### Step 3: Replace Registration API

**Before:**
```cpp
class EventSource {
    ValueChangedCallback mCallback = nullptr;
    void* mContext = nullptr;
public:
    void setCallback(ValueChangedCallback cb, void* ctx) {
        mCallback = cb;
        mContext = ctx;
    }
    
    void notifyValueChanged(int oldVal, int newVal) {
        if (mCallback) mCallback(mContext, oldVal, newVal);
    }
};
```

**After:**
```cpp
class EventSource {
public:
    ValueChangedSignal onValueChanged;  // Public signal
    
    void setValue(int newVal) {
        int oldVal = mValue;
        mValue = newVal;
        onValueChanged.emit(oldVal, newVal);
    }
private:
    int mValue = 0;
};
```

### Step 4: Update Subscriber Code

**Before:**
```cpp
class Observer {
    static void callback(void* ctx, int oldVal, int newVal) {
        Observer* self = static_cast<Observer*>(ctx);
        self->handleChange(oldVal, newVal);
    }
    
    EventSource* mSource = nullptr;
    
public:
    void observe(EventSource* src) {
        mSource = src;
        src->setCallback(callback, this);
    }
    
    ~Observer() {
        if (mSource) mSource->setCallback(nullptr, nullptr);
    }
    
    void handleChange(int oldVal, int newVal);
};
```

**After:**
```cpp
class Observer {
    ScopedConnection mConnection;
    
public:
    void observe(EventSource& src) {
        mConnection = src.onValueChanged.connect(
            [this](int oldVal, int newVal) {
                handleChange(oldVal, newVal);
            }
        );
    }
    
    // No destructor needed! ScopedConnection handles it
    
    void handleChange(int oldVal, int newVal);
};
```

### Step 5: Handle Thread Safety

If callbacks were called from multiple threads, use `ThreadSafeSignal`:

```cpp
// Before: manual mutex
class ThreadSafeSource {
    std::mutex mMutex;
    std::vector<std::pair<Callback, void*>> mCallbacks;
public:
    void addCallback(Callback cb, void* ctx) {
        std::lock_guard lock(mMutex);
        mCallbacks.emplace_back(cb, ctx);
    }
    void notify(int event) {
        std::lock_guard lock(mMutex);
        for (auto& [cb, ctx] : mCallbacks) cb(ctx, event);
    }
};

// After: built-in thread safety
using EventSignal = fat_p::ThreadSafeSignal<void(int)>;

class ThreadSafeSource {
public:
    EventSignal onEvent;  // Thread-safe by policy
};
```

---

## Before/After Examples

### Example 1: Simple Value Observer

**Before (C-style callback):**
```c
typedef void (*ValueCallback)(void* user, int value);

struct Sensor {
    ValueCallback callback;
    void* user_context;
    int current_value;
};

void sensor_set_callback(Sensor* s, ValueCallback cb, void* ctx) {
    s->callback = cb;
    s->user_context = ctx;
}

void sensor_update(Sensor* s, int new_value) {
    s->current_value = new_value;
    if (s->callback) {
        s->callback(s->user_context, new_value);
    }
}

// Usage
void my_callback(void* user, int value) {
    int* counter = (int*)user;  // Hope this cast is right!
    (*counter)++;
}

int counter = 0;
sensor_set_callback(&sensor, my_callback, &counter);
// What if counter goes out of scope?
```

**After (Signal):**
```cpp
class Sensor {
    int mCurrentValue = 0;
public:
    Signal<void(int)> onValueChanged;
    
    void update(int newValue) {
        mCurrentValue = newValue;
        onValueChanged.emit(newValue);
    }
};

// Usage
class Display {
    ScopedConnection mConnection;
    int mUpdateCount = 0;
public:
    void watchSensor(Sensor& sensor) {
        mConnection = sensor.onValueChanged.connect([this](int value) {
            mUpdateCount++;
            render(value);
        });
    }
    // Automatic cleanup when Display is destroyed
};
```

### Example 2: Multiple Listeners

**Before (manual list):**
```c
#define MAX_LISTENERS 8

struct EventSystem {
    struct {
        void (*callback)(void*, int);
        void* context;
    } listeners[MAX_LISTENERS];
    int count;
};

int add_listener(EventSystem* sys, void (*cb)(void*, int), void* ctx) {
    if (sys->count >= MAX_LISTENERS) return -1;
    sys->listeners[sys->count].callback = cb;
    sys->listeners[sys->count].context = ctx;
    return sys->count++;
}

void fire_event(EventSystem* sys, int event_type) {
    for (int i = 0; i < sys->count; i++) {
        sys->listeners[i].callback(sys->listeners[i].context, event_type);
    }
}
```

**After (Signal):**
```cpp
class EventSystem {
public:
    Signal<void(int eventType)> onEvent;
    
    void fireEvent(int eventType) {
        onEvent.emit(eventType);
    }
};

// Unlimited listeners, automatic cleanup
class Logger {
    ScopedConnection mConn;
public:
    void attach(EventSystem& sys) {
        mConn = sys.onEvent.connect([](int e) { log("Event: {}", e); });
    }
};

class Analytics {
    ScopedConnection mConn;
public:
    void attach(EventSystem& sys) {
        mConn = sys.onEvent.connect([this](int e) { recordEvent(e); });
    }
};
```

### Example 3: Reentrancy-Safe Disconnect

**Before (broken):**
```c
void problematic_callback(void* ctx, int event) {
    ListenerSystem* sys = (ListenerSystem*)ctx;
    
    if (event == SHUTDOWN_EVENT) {
        // This corrupts iteration in fire_event!
        remove_listener(sys, problematic_callback);
    }
}
```

**After (safe):**
```cpp
class ShutdownHandler {
    ScopedConnection mConn;
public:
    void attach(Signal<void(int)>& signal) {
        mConn = signal.connect([this](int event) {
            if (event == SHUTDOWN_EVENT) {
                mConn.disconnect();  // Safe! Signal handles deferred cleanup
            }
        });
    }
};
```

### Example 4: Priority-Based Ordering

**Before (manual ordering):**
```c
struct PriorityCallback {
    void (*callback)(void*, int);
    void* context;
    int priority;
};

// Must maintain sorted order manually
void add_prioritized(System* sys, void (*cb)(void*, int), void* ctx, int prio) {
    // Insert in sorted position... error-prone
}
```

**After (built-in priority):**
```cpp
Signal<void(int)> signal;

// Higher priority = called first
auto high = signal.connect([](int v) { log("High priority"); }, 100);
auto normal = signal.connect([](int v) { log("Normal"); }, 0);
auto low = signal.connect([](int v) { log("Low priority"); }, -100);

signal.emit(42);
// Output:
// High priority
// Normal
// Low priority
```

---

## Advanced Patterns

### Pattern: Collecting Return Values

```cpp
// Signal with return value
Signal<bool(const Request&)> onValidate;

// Connect validators
onValidate.connect([](const Request& r) { return r.hasAuth(); });
onValidate.connect([](const Request& r) { return r.size() < MAX_SIZE; });

// Collect all results
auto results = onValidate.emitCollect(request);
bool allValid = std::all_of(results.begin(), results.end(), 
                            [](bool v) { return v; });

// Or: short-circuit on first failure
bool valid = onValidate.emitUntil(request);  // Stops on first 'true'
```

### Pattern: Connection Groups

```cpp
class Screen {
    std::vector<ScopedConnection> mConnections;
    
public:
    void activate(GameState& state) {
        mConnections.push_back(state.onPlayerMove.connect(...));
        mConnections.push_back(state.onEnemySpawn.connect(...));
        mConnections.push_back(state.onScoreChange.connect(...));
    }
    
    void deactivate() {
        mConnections.clear();  // Disconnect all at once
    }
};
```

### Pattern: Manual Connection ID

For cases where RAII isn't appropriate:

```cpp
class LongLivedObserver {
    ConnectionId mConnId;
    Signal<void(int)>* mSignal = nullptr;
    
public:
    void connect(Signal<void(int)>& sig) {
        mSignal = &sig;
        mConnId = sig.connectManual([this](int v) { handle(v); });
    }
    
    void disconnect() {
        if (mSignal && mSignal->isConnected(mConnId)) {
            mSignal->disconnect(mConnId);
        }
        mSignal = nullptr;
    }
};
```

### Pattern: Weak Reference Safety

Prevent calling into destroyed objects:

```cpp
class SafeObserver : public std::enable_shared_from_this<SafeObserver> {
    ScopedConnection mConn;
    
public:
    void observe(Signal<void(int)>& sig) {
        std::weak_ptr<SafeObserver> weak = shared_from_this();
        
        mConn = sig.connect([weak](int value) {
            if (auto self = weak.lock()) {
                self->handleValue(value);
            }
        });
    }
};
```

---

## Thread Safety

### Single-Threaded (Default)

```cpp
Signal<void(int)> signal;  // Uses SingleThreadedPolicy
// Fast, no locking overhead
// Only safe if all access is from one thread
```

### Thread-Safe Signals

```cpp
// Full mutex protection
ThreadSafeSignal<void(int)> signal;

// From any thread:
auto conn = signal.connect([](int v) { ... });  // Safe
signal.emit(42);                                  // Safe
conn.disconnect();                                // Safe
```

### Spinlock for Low-Latency

```cpp
// For very short callbacks, spinlock avoids syscall overhead
SpinlockSignal<void(int)> signal;
```

### Choosing a Policy

| Scenario | Policy | Reason |
|----------|--------|--------|
| UI thread only | `SingleThreadedPolicy` | No overhead |
| Worker → UI thread | `SharedMutexPolicy` | Safe, good read performance |
| High-frequency trading | `SpinlockSynchronizationPolicy` | Low latency |
| Unknown threading | `SharedMutexPolicy` | Safe default |

---

## Verification

### Compile-Time Verification

Signal provides type safety. These should fail to compile:

```cpp
Signal<void(int, std::string)> sig;

// Wrong number of arguments:
sig.emit(42);                        // ERROR
sig.emit(42, "hello", 3.14);         // ERROR

// Wrong argument types:
sig.emit("hello", 42);               // ERROR (swapped)
sig.emit(42, 123);                   // ERROR (int, not string)

// Wrong callable signature:
sig.connect([](float f) { });        // ERROR
sig.connect([](int i, int j) { });   // ERROR
```

### Runtime Verification

```cpp
TEST(SignalMigration, BasicEmission) {
    Signal<void(int)> sig;
    int received = 0;
    
    auto conn = sig.connect([&](int v) { received = v; });
    
    sig.emit(42);
    EXPECT_EQ(received, 42);
}

TEST(SignalMigration, MultipleListeners) {
    Signal<void(int)> sig;
    std::vector<int> values;
    
    auto c1 = sig.connect([&](int v) { values.push_back(v * 1); });
    auto c2 = sig.connect([&](int v) { values.push_back(v * 2); });
    auto c3 = sig.connect([&](int v) { values.push_back(v * 3); });
    
    sig.emit(10);
    EXPECT_EQ(values, (std::vector<int>{10, 20, 30}));
}

TEST(SignalMigration, AutomaticDisconnect) {
    Signal<void(int)> sig;
    int count = 0;
    
    {
        auto conn = sig.connect([&](int) { count++; });
        sig.emit(1);
        EXPECT_EQ(count, 1);
    }  // conn destroyed, disconnected
    
    sig.emit(1);
    EXPECT_EQ(count, 1);  // Callback not called
}

TEST(SignalMigration, DisconnectDuringEmit) {
    Signal<void(int)> sig;
    ScopedConnection conn;
    int count = 0;
    
    conn = sig.connect([&](int) {
        count++;
        conn.disconnect();  // Disconnect self during emission
    });
    
    sig.emit(1);
    EXPECT_EQ(count, 1);
    
    sig.emit(1);
    EXPECT_EQ(count, 1);  // Already disconnected
}

TEST(SignalMigration, Priority) {
    Signal<void()> sig;
    std::string order;
    
    sig.connect([&]() { order += "B"; }, 0);
    sig.connect([&]() { order += "A"; }, 100);  // Higher priority
    sig.connect([&]() { order += "C"; }, -100); // Lower priority
    
    sig.emit();
    EXPECT_EQ(order, "ABC");
}
```

### Thread-Safety Verification

```cpp
TEST(SignalMigration, ThreadSafety) {
    ThreadSafeSignal<void(int)> sig;
    std::atomic<int> sum{0};
    
    auto conn = sig.connect([&](int v) {
        sum += v;
    });
    
    std::vector<std::thread> threads;
    for (int i = 0; i < 100; ++i) {
        threads.emplace_back([&sig, i]() {
            sig.emit(i);
        });
    }
    
    for (auto& t : threads) t.join();
    
    // All emissions should be received
    EXPECT_EQ(sum.load(), 100 * 99 / 2);  // Sum of 0..99
}
```

---

## When Signal Loses

### 1. C ABI Requirements

If you must expose callbacks across a C interface (plugins, FFI):

```c
// Signal can't help here; use C-style callbacks at the boundary
extern "C" void set_plugin_callback(void (*cb)(void*, int), void* ctx);
```

**Mitigation:** Use Signal internally, bridge to C callbacks at the boundary:

```cpp
Signal<void(int)> internalSignal;

extern "C" void set_plugin_callback(void (*cb)(void*, int), void* ctx) {
    internalSignal.connect([cb, ctx](int v) {
        cb(ctx, v);
    });
}
```

### 2. Extreme Performance Requirements

For millions of emissions per second with single callback:

```cpp
// Raw function pointer: ~1ns
// Signal with 1 listener: ~5-10ns
// The difference usually doesn't matter, but measure if critical
```

**Mitigation:** Use `std::function` or raw function pointer for proven hot paths.

### 3. Return Value Aggregation Complexity

Signal supports simple return collection, but complex aggregation (voting, weighted average) needs custom logic:

```cpp
// This works:
auto results = sig.emitCollect(args);
auto winner = std::max_element(results.begin(), results.end());

// But complex voting logic is better in a custom class
```

### 4. Very Large Number of Signals

Each Signal instance has some overhead (~48 bytes minimum). If you have millions of independent events, consider a centralized event bus pattern instead.

---

## Summary

| Aspect | C Callbacks | Signal |
|--------|-------------|--------|
| Type safety | None (`void*`) | Full (compile-time) |
| Multiple listeners | Manual | Built-in |
| Automatic cleanup | No | Yes (RAII) |
| Disconnect during emit | Dangerous | Safe (deferred cleanup) |
| Thread safety | Manual | Policy-based |
| Priority ordering | Manual | Built-in |
| Heap allocation | N/A | Zero for ≤4 listeners |

**Migration ROI:**
- **Immediate:** Eliminate dangling callback crashes
- **Short-term:** Cleaner code, less boilerplate
- **Long-term:** Safer refactoring, better testing

The Signal pattern replaces error-prone manual callback management with a type-safe, RAII-based system that handles the hard problems (lifetime, reentrancy, threading) correctly.

---

## References

- [SQLite callback patterns](https://github.com/sqlite/sqlite/blob/master/src/main.c) — C-style callbacks in practice
- Fat-P User Manual: Signal — Complete API reference
- Fat-P User Manual: ScopedConnection — RAII connection management
- Foundations: ABI Stability — When C callbacks are required

---

*FAT-P Library Documentation — January 2025*
