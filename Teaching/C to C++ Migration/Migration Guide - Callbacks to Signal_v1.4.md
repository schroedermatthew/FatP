---
doc_id: MG-SIGNAL-001
doc_type: "Migration Guide"
title: "Migration Guide - Callbacks to Signal"
fatp_components: ["Signal"]
topics:
  - "c-to-cpp"
  - "migration"
  - "callbacks"
  - "observer"
  - "signal"
  - "lifetime"
  - "c-abi"
constraints:
  - "C ABI boundary"
  - "lifetime"
  - "thread-safety"
cxx_standard: "C++20"
audience:
  - "C developers"
  - "C++ developers"
  - "AI assistants"
status: "draft"
last_verified: "2026-01-09"

from_language: "C"
to_language: "C++"
to_standard: "C++20"
from_pattern:
  - "function pointers"
  - "callback + context pairs"
  - "manual observer lists"
to_component:
  - "Signal"
boost_equivalent: "Boost.Signals2"
migration_complexity: "Medium"
breaking_changes: true
---

# Migration Guide - Callbacks to Signal

### *Three ways to escape `void (*callback)(void*)`*

*FAT-P Library — January 2026*

---

## Scope

This guide targets **existing C or C-style C++ code** that uses `void*` context pointers plus function-pointer callbacks
(or manual observer lists) and migrates that surface area to a **type-safe observer model** using `Signal`.

The goal is to preserve behavior while making **lifetime, ownership, threading, and teardown** explicit in the API.

## Not covered

- Converting a callback-based design into an **async** model (futures, coroutines, executors)
- Cross-process or networked event delivery (IPC, RPC, pub/sub brokers)
- Code-generation based systems (e.g., Qt MOC signals/slots)
- Full performance engineering for hard real-time or ultra-hot inner loops (see “When Signal Loses”)

## Prerequisites

- You can compile as **C++17**
- You can change the public API at the migration boundary (this is a breaking-change migration)
- You can identify (and write tests for) the **behavioral contract** of the existing callbacks:
  ordering, reentrancy, lifetime, and threading expectations

## Migration Guide Card

**From:** Function pointers, callback + context pairs (`void*`), manual observer lists  
**To:** `Signal` observers with explicit connection lifetime (`ScopedConnection`)  
**Why migrate:** Remove implicit contracts (casts, lifetime, teardown, reentrancy) from “tribal knowledge” and make them
enforceable in types and tests.  
**Compatibility strategy:** Keep any required **C ABI** at the boundary; use Signal internally behind a C-facing wrapper.  
**Mechanical steps:**
1. Identify each callback registration point and its lifetime/threading expectations.
2. Replace “register/unregister” pairs with `connect()` returning a stored `ScopedConnection`.
3. Replace `void*` context with captured references/pointers whose lifetime is tied to the connection owner.
4. Add verification tests (ordering, reentrancy, disconnect-during-emit, threading) before deleting the old path.
**Behavioral equivalence:** Same observable events, same payload values, and (if required) the same ordering contract.  
**Intentional differences:** Connection lifetime is explicit; teardown becomes automatic via RAII.  
**Failure model:** C-style error returns remain at the C boundary; internal code can use C++ error handling as appropriate.  
**Threading model:** Explicitly defined per Signal policy (single-threaded vs synchronized).  
**Lifetime model:** The object that owns the `ScopedConnection` owns the subscription.  
**Alternatives:** DIY (`std::function` + manual list), Boost.Signals2, other signal/slot libraries.  
**Verification:** Unit tests for ordering, reentrancy, disconnect-during-emit, and (if applicable) concurrency.  
**Rollback plan:** Keep the old callback registration path behind a feature flag until parity tests pass in production.

---

## Alternatives

- **Boost.Signals2** — Mature, feature-complete, part of Boost
- **libsigc++** — GTK's signal library, widely used in GNOME ecosystem
- **nano-signal-slot** — Header-only, minimal implementation
- **Wink-Signals** — Header-only, focus on performance
- **sigslot** — Sarah Thompson's original, public domain
- **Qt Signals/Slots** — Requires MOC preprocessor, Qt ecosystem only

---

## Table of Contents

1. [The Problem with C-Style Callbacks](#the-problem-with-c-style-callbacks)
2. [Real-World Callback Disasters](#real-world-callback-disasters)
3. [The C Patterns](#the-c-patterns)
4. [The Signal Solution](#the-signal-solution)
5. [Three Migration Paths](#three-migration-paths)
6. [Migration Examples](#migration-examples)
   - The Lifetime Problem
   - The Reentrancy Problem
   - The Threading Problem
   - The Aggregation Problem
   - When the Differences Matter
7. [Choosing Your Path](#choosing-your-path)
8. [Advanced Patterns](#advanced-patterns) *(Fat-P specific)*
9. [Thread Safety](#thread-safety) *(Fat-P specific)*
10. [Verification](#verification) *(Fat-P specific)*
11. [When Signal Loses](#when-signal-loses)

---

## Mapping: From → To

| C-style callback surface | Typical problems | Signal-based equivalent |
|---|---|---|
| `register_callback(cb, ctx)` | `void*` casts, lifetime mismatch | `auto c = sig.connect([&](Args...) { ... });` |
| `unregister_callback(cb, ctx)` | missed unregister, double-unregister | `c.disconnect();` or RAII teardown |
| manual observer list | iterator invalidation, reentrancy bugs | internal connection list with defined rules |
| implicit ordering | “works by accident” | explicit priority / order policy (if provided) |

This guide keeps the “big picture” sections below (problem → patterns → three migration paths), but the sections here
are the **operational migration checklist** you can execute.

## Step-by-step migration plan

1. **Freeze behavior**: write tests for the old callback system (ordering, reentrancy, teardown, threading).
2. **Define the boundary**: decide where C API ends and C++ begins (especially for plugins).
3. **Introduce Signal internally** behind an adapter that still satisfies the old API.
4. **Move each callback site** to `connect()` and store the returned `ScopedConnection` in the owning object/state.
5. **Delete manual unregister** paths once the owning state lifetime fully controls teardown.
6. **Remove the old API** (or keep it as a thin wrapper) once production parity is proven.

## Compatibility and ABI boundaries

If you must expose a **C ABI** (plugins, shared libraries, C-only toolchains), keep the C callback at the edge and bridge
to Signal internally. The bridge must persist the connection handle; otherwise the subscription ends immediately.

See **“When Signal Loses → C ABI Requirements”** for a boundary adapter example.

## Lifetime and ownership model

- The owner of the `ScopedConnection` owns the subscription.
- If you want “disconnect before other members are destroyed,” declare the `ScopedConnection` member **after** the members
  it depends on (members are destroyed in reverse declaration order).

## Thread-safety and reentrancy

- Thread-safety depends on the Signal’s chosen policy. Treat it as a contract: either single-threaded or synchronized.
- Reentrancy rules must be explicit: disconnect-during-emit and connect-during-emit behavior should be covered by tests.

## Error and failure model

Map C behavior explicitly:
- **C**: return codes / `errno` / out-parameters / callbacks returning non-zero to stop iteration
- **C++**: exceptions, `Expected`-style return types, or error-code returns

At the C ABI boundary, keep the C error model; translate internally as needed.

## Verification plan

Minimum set:
- ordering (if applicable)
- reentrancy (connect/disconnect during emit)
- lifetime teardown (scope-based auto-disconnect)
- concurrency (if enabled): emit from multiple threads or connect/disconnect concurrently

Treat any micro-benchmarking as a sanity check, not a substitute for a dedicated benchmark suite.

## Rollback plan

- Keep the legacy callback path behind a runtime flag or build-time switch during rollout.
- Run parity tests in CI and in a canary environment before removing the old path.
- If regressions appear, flip back to the old path without changing the public ABI.

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

Before we can migrate, we need to recognize what we're migrating from. C callback patterns evolved over decades of systems programming, and each variant solves a real problem while introducing new ones.

### Pattern 1: Function Pointer + Context

The simplest callback pattern stores a function pointer and a `void*` for user data. You see this everywhere: POSIX signal handlers, OpenGL callbacks, embedded system interrupts.

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

This pattern is minimal and fast. It compiles to a handful of instructions. But it limits you to one callback per source, which means the last caller to `set_callback` wins. If two subsystems both want to observe the same event, one of them loses silently.

The `void*` context is a necessary evil—C has no closures—but it's a type-safety hole. Nothing stops you from passing a `Widget*` and casting it to `AudioBuffer*` in the callback. The compiler won't complain; you'll just corrupt memory.

And lifetime? Entirely your problem. The event source stores a raw pointer. If the context object is freed before the callback fires, you get a crash—or worse, silent corruption.

### Pattern 2: Callback List with Manual Management

When one callback isn't enough, you build a list:

```c
#define MAX_CALLBACKS 16

struct CallbackEntry {
    Callback cb;
    void* context;
    int id;
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
            memmove(&src->callbacks[i], &src->callbacks[i+1], 
                    (src->count - i - 1) * sizeof(CallbackEntry));
            src->count--;
            return;
        }
    }
}
```

Now you can have multiple observers, but you've introduced new problems. The fixed-size array means you pick a magic number (16? 64? 256?) and hope it's enough. Dynamic allocation solves that but adds complexity and potential failure points.

Removal is O(n) and requires the caller to save the ID returned from `add_callback`. Forget to save it, and you can never unsubscribe.

The real danger is reentrancy. What happens if a callback calls `remove_callback` while the event loop is iterating through the list? The `memmove` shifts elements, and the loop either skips a callback or calls one twice. This bug is notoriously hard to reproduce because it depends on callback order and timing.

### Pattern 3: Linked List of Observers

Linked lists avoid the fixed-size problem and make removal O(1) if you have the node pointer:

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
        Observer* next = o->next;  // Cache before callback
        o->notify(o, event);
        o = next;
    }
}
```

The caching of `next` before invoking the callback is a partial defense against reentrancy—if the callback detaches itself, we still have the next pointer. But it doesn't help if the callback attaches new observers or detaches *other* observers.

This pattern also forces an intrusive design: the observer must contain the `Observer` struct as a member. You can't observe the same event with two different callbacks from one object without wrapper structs.

### Pattern 4: Function Tables (VTables)

When you have multiple related callbacks, you bundle them into a struct:

```c
struct sqlite3_vfs {
    int iVersion;
    int (*xOpen)(sqlite3_vfs*, const char*, sqlite3_file*, int, int*);
    int (*xDelete)(sqlite3_vfs*, const char*, int);
    int (*xAccess)(sqlite3_vfs*, const char*, int, int*);
    int (*xFullPathname)(sqlite3_vfs*, const char*, int, char*);
    /* ... more function pointers ... */
};
```

This is SQLite's VFS (Virtual File System) interface. It's essentially a manual vtable—C's way of approximating polymorphism. It works well for plugin architectures where a single entity implements many related operations.

But it's overkill for simple event notification. You don't want to define a 20-function struct just to listen for value changes. And the lifetime/type-safety problems remain.

### The Common Thread

All these patterns share the same fundamental weaknesses:

The type system can't help you. Every `void*` cast is a prayer that you got it right. Every callback registration is a promise to unregister before destruction—a promise the compiler can't verify.

Reentrancy is an afterthought. Each pattern requires careful defensive coding to handle modification during iteration, and most real-world implementations get it wrong in subtle ways.

Thread safety is absent. Adding it means adding mutexes, but then callbacks might deadlock if they try to register or unregister other callbacks.

These aren't bugs in specific implementations. They're limitations of the callback model itself. The C++ solutions we'll examine don't just provide nicer syntax—they restructure the problem so these failure modes become impossible.

---

## The Signal Solution

The Signal pattern inverts the callback model. Instead of pushing a function pointer and context into an event source, observers pull a connection handle out of it. That handle is an RAII object—when it's destroyed, the callback is automatically unregistered.

This simple inversion eliminates entire categories of bugs. Dangling pointers become impossible because the connection handle's destructor runs before the observer's members are destroyed. Reentrancy is handled internally because the signal defers disconnection until emission completes. Type safety is enforced at compile time because the signal's template parameter specifies the exact callback signature.

Here's what it looks like:

```cpp
#include "Signal.h"
using namespace fat_p;

Signal<void(int)> onValueChanged;

auto conn1 = onValueChanged.connect([](int v) {
    std::cout << "Value: " << v << "\n";
});

auto conn2 = onValueChanged.connect([](int v) {
    log_value(v);
});

onValueChanged.emit(42);  // Both callbacks fire
// When conn1 and conn2 go out of scope, they disconnect automatically
```

The `Signal<void(int)>` declaration specifies that listeners must accept a single `int` argument. If you try to connect a lambda with the wrong signature, the compiler rejects it. No more casting from `void*` and hoping.

The `connect()` method returns a `ScopedConnection`, an RAII handle that unregisters the callback when destroyed. You store it as a member variable, and cleanup happens automatically. If you forget to store it, the callback disconnects immediately—a loud, obvious failure during development rather than a silent corruption in production.

### Connection Lifetime

The connection handle solves the hardest problem in callback systems: knowing when to unregister. In C, you need the observer to store a reference to the event source, implement an `unsubscribe()` method, and call it from the destructor. Miss any step and you have a dangling pointer.

With `ScopedConnection`, the destructor does the right thing automatically:

```cpp
class Widget {
    ScopedConnection mConnection;
    
public:
    void subscribeTo(Signal<void(int)>& signal) {
        mConnection = signal.connect([this](int v) {
            handleValue(v);  // Uses 'this' safely
        });
    }
    
    // No destructor needed—mConnection's destructor disconnects
};
```

When the `Widget` is destroyed, `mConnection` is destroyed first (C++ destroys members in reverse declaration order). The destructor unregisters the callback before `Widget`'s other members are destroyed. By the time the lambda's captured `this` would become invalid, the callback is already gone.

This isn't magic—it's the same RAII pattern that makes `std::unique_ptr` work. But applying it to callbacks requires library support that C doesn't have and std:: doesn't provide.

---

## Three Migration Paths

You have three practical options for replacing C-style callbacks. This section shows the same C code migrated three ways so you can compare.

### Option 1: Standard Library (DIY)

Build your own with `std::function` and `std::vector`. No external dependencies, but you handle everything yourself.

### Option 2: Boost.Signals2

The mature, feature-complete solution. Requires Boost. Offers automatic disconnection via `trackable` and return value combiners.

### Option 3: Fat-P Signal

Single-header, no dependencies. Policy-based thread safety. Small-object optimization avoids heap allocation for small listener counts.

---

## Migration Examples

Each example starts with a C callback pattern and walks through how each option handles it. The goal isn't to show complete implementations—it's to show how each approach thinks about the problem differently.

### The Lifetime Problem

The most dangerous C callback bug is the dangling pointer. You register a callback with a context pointer, the object behind that pointer gets destroyed, and the next event fires into garbage memory.

```c
void my_handler(void* ctx, int value) {
    Widget* w = (Widget*)ctx;  // Is this still valid?
    w->update(value);           // Crash if widget was destroyed
}

sensor_set_callback(&sensor, my_handler, &widget);
// ... later, widget is destroyed but callback remains registered
```

The C programmer must remember to unregister before destruction. In practice, this means storing a reference to the event source in every observer, adding an `unsubscribe()` method, and calling it from every destructor. It's tedious and error-prone.

**The std:: approach** doesn't solve this automatically. You can wrap callbacks in `std::function` and store them in a vector, but you still need manual tracking. When the observer is destroyed, nothing automatically removes its callback from the source's list. You're in the same boat as C, just with nicer syntax.

**Boost.Signals2** solves this with RAII. The `connect()` call returns a connection object. Store it as a member, and when the observer is destroyed, the connection's destructor unregisters the callback automatically:

```cpp
class Observer {
    boost::signals2::scoped_connection mConn;
public:
    void watch(Sensor& s) {
        mConn = s.onValueChanged.connect([this](int v) { handle(v); });
    }
    // Destructor destroys mConn, which disconnects automatically
};
```

**Fat-P Signal** works the same way:

```cpp
class Observer {
    ScopedConnection mConn;
public:
    void watch(Sensor& s) {
        mConn = s.onValueChanged.connect([this](int v) { handle(v); });
    }
};
```

The Boost and Fat-P solutions are nearly identical here. Both turn a runtime discipline problem into a compile-time structural guarantee. If you forget to store the connection, the callback disconnects immediately—a loud failure you'll notice during development, not a silent one that corrupts memory in production.

---

### The Reentrancy Problem

What happens when a callback tries to modify the listener list during iteration?

```c
void shutdown_handler(void* ctx, int event) {
    if (event == SHUTDOWN) {
        remove_listener(global_system, shutdown_handler);  // Corrupts iteration!
    }
}
```

In C, this is undefined behavior. The `fire_event` loop is walking through an array that just changed underneath it. Some implementations crash. Others skip callbacks or call them twice. The bug is intermittent and hard to reproduce.

**The std:: approach** requires you to handle this explicitly. One common pattern copies the callback list before iteration:

```cpp
void fire(int event) {
    auto copy = mListeners;  // Copy the vector
    for (auto& cb : copy) {
        cb(event);
    }
}
```

This works but has overhead, and you still need to handle the case where a callback adds new listeners—should they fire in this round or not? You're reimplementing event system semantics from scratch.

**Boost.Signals2 and Fat-P Signal** both handle this internally. You can disconnect during a callback—even disconnect yourself—and the library defers the actual removal until iteration completes. The semantics are well-defined: removed callbacks won't fire again in this emission, newly added callbacks won't fire until the next emission.

```cpp
mConn = signal.connect([this](int event) {
    if (event == SHUTDOWN) {
        mConn.disconnect();  // Safe in both Boost and Fat-P
    }
});
```

You don't need to think about it. The library handles the bookkeeping.

---

### The Threading Problem

C callbacks in multithreaded code require manual synchronization. You add mutexes around registration and emission, but then callbacks can deadlock if they try to register or unregister other callbacks:

```c
void fire_event(System* sys, int event) {
    pthread_mutex_lock(&sys->mutex);
    for (int i = 0; i < sys->count; i++) {
        // If this callback tries to add/remove listeners, deadlock
        sys->callbacks[i](sys->contexts[i], event);
    }
    pthread_mutex_unlock(&sys->mutex);
}
```

The solution is to copy callbacks under the lock, release the lock, then call them—but now you're back to manual reentrancy handling.

**The std:: approach** means implementing all of this yourself. It's doable but subtle. Most DIY implementations either deadlock under contention or have race conditions on registration.

**Boost.Signals2** is thread-safe by default. Every operation takes a lock internally. This is safe but means you pay mutex overhead even in single-threaded code.

**Fat-P Signal** makes thread safety a policy choice:

```cpp
fat_p::Signal<void(int)> fast;              // No locking, single-thread only
fat_p::ThreadSafeSignal<void(int)> safe;    // Mutex-protected
```

If you know your signal is only used from one thread—common for UI code—you pay nothing. If you need thread safety, you opt in explicitly. This matches the C++ philosophy of not paying for what you don't use.

---

### The Aggregation Problem

Sometimes callbacks return values and you need to combine them—voting systems, validation chains, priority overrides. In C, you handle this manually:

```c
bool validate_all(Validators* v, Request* req) {
    for (int i = 0; i < v->count; i++) {
        if (!v->validators[i](req)) return false;  // Short-circuit
    }
    return true;
}
```

**The std:: approach** is similar—loop and combine manually.

**Boost.Signals2** has first-class support for this via combiners. You can specify how return values are aggregated when you declare the signal:

```cpp
boost::signals2::signal<bool(Request), boost::signals2::last_value<bool>> sig;
// Returns the last slot's return value

boost::signals2::signal<int(Request), maximum<int>> sig;
// Returns the maximum of all slot return values
```

This is powerful but adds complexity. Most use cases don't need it.

**Fat-P Signal** provides basic collection:

```cpp
auto results = signal.emitCollect(request);  // Returns vector of results
bool allPassed = std::all_of(results.begin(), results.end(), 
                              [](bool b) { return b; });
```

You do the aggregation yourself. This is less elegant than Boost's combiners but covers most practical cases.

---

### When the Differences Matter

For simple observer patterns—UI events, sensor updates, game object notifications—Boost and Fat-P are functionally equivalent. The code looks almost identical, and both solve the lifetime and reentrancy problems correctly.

The differences matter at the edges:

**Choose std:: DIY** if you have trivial needs (one or two callbacks, no dynamic registration), want zero dependencies, and are comfortable implementing safety guarantees yourself.

**Choose Boost.Signals2** if you're already using Boost, need the `trackable` mixin for automatic disconnection based on observed object lifetime, or need sophisticated combiner patterns.

**Choose Fat-P Signal** if you want the safety of Boost without the dependency weight, need policy-based thread safety, or are counting allocations in hot paths.

---

## Choosing Your Path

For quick reference:

| Consideration | std:: DIY | Boost.Signals2 | Fat-P Signal |
|---------------|-----------|----------------|--------------|
| Dependencies | None | Boost | None |
| Automatic disconnect | No | Yes | Yes |
| Reentrancy safety | Manual | Built-in | Built-in |
| Thread safety | Manual | Always on | Policy-based |
| `trackable` mixin | No | Yes | No |
| Return combiners | No | Yes | Basic |
| Compile time | Fast | Slow | Fast |

---

The remaining sections cover Fat-P Signal specifics: advanced usage patterns, thread safety policies, and testing strategies. If you chose Boost.Signals2, their [tutorial](https://www.boost.org/doc/libs/release/doc/html/signals2/tutorial.html) covers similar ground.

---

## Advanced Patterns

Once you've migrated from C callbacks to Signal, you'll encounter patterns that go beyond simple event notification. These patterns apply to Fat-P Signal but the concepts translate to Boost.Signals2 with minor syntax changes.

### Collecting Return Values

Sometimes callbacks need to vote. A validation chain checks multiple conditions; an auction system collects bids; a plugin architecture asks each plugin if it can handle a request. In C, you'd loop through callbacks and aggregate results manually. Signal provides `emitCollect()` to gather all return values into a container:

```cpp
Signal<bool(const Request&)> onValidate;

onValidate.connect([](const Request& r) { return r.hasAuth(); });
onValidate.connect([](const Request& r) { return r.size() < MAX_SIZE; });

auto results = onValidate.emitCollect(request);
bool allValid = std::all_of(results.begin(), results.end(), 
                            [](bool v) { return v; });
```

For short-circuit evaluation—stop as soon as one validator fails—use `emitUntil()`:

```cpp
bool valid = onValidate.emitUntil(request, [](bool v) { return !v; });
```

Boost.Signals2 has more sophisticated combiners for this, but `emitCollect()` covers most real-world cases.

### Connection Groups

UI code often has screens or panels that connect to many signals when activated and disconnect when deactivated. Managing individual connections gets tedious. Instead, collect them in a vector:

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
        mConnections.clear();  // All connections disconnect at once
    }
};
```

When `deactivate()` clears the vector, each `ScopedConnection`'s destructor runs, disconnecting all callbacks in one operation. This pattern also works for tab controls, modal dialogs, or any component with distinct active/inactive states.

### Manual Connection ID

RAII connections are the right default, but sometimes you need manual control. Long-lived singletons, static objects, or callbacks that should survive their registration scope—these cases call for `connectManual()`:

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

With manual connections, you're back to C-style responsibility: forget to disconnect and you have a dangling callback. Use this sparingly, and only when RAII genuinely doesn't fit.

### Weak Reference Safety

When observers might be destroyed from another thread or through shared ownership, the connection handle alone isn't enough. The callback fires, the destructor runs concurrently, and you're back to racing. Weak pointers solve this:

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

The lambda captures a weak pointer, not `this`. When the callback fires, `weak.lock()` either returns a valid `shared_ptr` (extending the object's lifetime for the duration of the call) or returns null (the object is already gone). Either way, no dangling pointer.

This pattern adds overhead and complexity. You don't need it for single-threaded code with clear ownership, but it's essential when shared_ptr already manages your object's lifetime.

---

## Thread Safety

The C callback sections showed how manual thread safety leads to deadlocks and races. Signal libraries solve this differently.

### Single-Threaded (Default)

Most signals live on one thread—the UI thread, the game loop, the main event processor. For these, locking is pure overhead:

```cpp
Signal<void(int)> signal;  // Uses SingleThreadedPolicy by default
```

No mutexes, no atomic operations, no cache line bouncing. If you violate the single-thread assumption, you get data races—but the code is honest about its requirements.

### Thread-Safe Signals

When signals cross thread boundaries—worker threads posting to the UI, network handlers updating shared state—use the thread-safe variant:

```cpp
ThreadSafeSignal<void(int)> signal;

// From any thread:
auto conn = signal.connect([](int v) { ... });  // Safe
signal.emit(42);                                  // Safe
conn.disconnect();                                // Safe
```

Internally, this wraps operations in a mutex. The implementation is careful to release the lock before invoking callbacks, avoiding the deadlock trap that naive mutex-around-everything approaches fall into.

### Spinlock for Low-Latency

For signals in hot paths—audio processing, high-frequency trading, game physics—mutex overhead matters. Spinlocks avoid kernel transitions at the cost of CPU spin when contended:

```cpp
SpinlockSignal<void(int)> signal;
```

This is a micro-optimization. Profile first. Most code should use `SharedMutexPolicy` and move on.

### Choosing a Policy

| Scenario | Policy | Reason |
|----------|--------|--------|
| UI thread only | `SingleThreadedPolicy` | No overhead |
| Worker → UI thread | `SharedMutexPolicy` | Safe, good read performance |
| High-frequency trading | `SpinlockSynchronizationPolicy` | Low latency |
| Unknown threading | `SharedMutexPolicy` | Safe default |

When in doubt, use `ThreadSafeSignal`. The overhead is negligible for most applications, and debugging race conditions is expensive.

---

## Verification

How do you know your migration worked? Signals provide verification at both compile time and runtime.

### Compile-Time Verification

The strongest guarantee is that wrong code won't compile. Signal's template signature enforces argument types and counts:

```cpp
Signal<void(int, std::string)> sig;

// Wrong number of arguments—compiler error:
sig.emit(42);                        // ERROR: missing string argument
sig.emit(42, "hello", 3.14);         // ERROR: too many arguments

// Wrong argument types—compiler error:
sig.emit("hello", 42);               // ERROR: arguments swapped
sig.emit(42, 123);                   // ERROR: int is not std::string

// Wrong callable signature—compiler error:
sig.connect([](float f) { });        // ERROR: wrong parameter type
sig.connect([](int i, int j) { });   // ERROR: wrong signature
```

In C, these errors would compile silently and crash at runtime (or worse, corrupt memory silently). With Signal, the compiler catches them before you run a single test.

### Runtime Verification

Compile-time checks catch signature mismatches but not logic errors. Unit tests verify that your migrated code behaves correctly:

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
```

The critical test is automatic disconnection—the behavior that eliminates dangling pointers:

```cpp
TEST(SignalMigration, AutomaticDisconnect) {
    Signal<void(int)> sig;
    int count = 0;
    
    {
        auto conn = sig.connect([&](int) { count++; });
        sig.emit(1);
        EXPECT_EQ(count, 1);
    }  // conn destroyed here, callback disconnected
    
    sig.emit(1);
    EXPECT_EQ(count, 1);  // Callback was NOT called
}
```

And the reentrancy test—the behavior that prevents iteration corruption:

```cpp
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

If you're using `ThreadSafeSignal`, test it under actual contention. This test fires the signal from 100 threads simultaneously:

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
    
    EXPECT_EQ(sum.load(), 100 * 99 / 2);  // Sum of 0..99
}
```

If this test fails with data races or wrong sums, you've either used the wrong policy or have a bug in your migration.

---

## When Signal Loses

Signal solves the common case beautifully, but no abstraction is universal. Here are the situations where you might choose something else.

### 1. C ABI Requirements

If your code must expose callbacks across a C interface—plugins loaded at runtime, FFI bindings for other languages, embedded systems with C-only toolchains—Signal can't cross that boundary directly:

```c
// This is a C header. Signal doesn't exist here.
extern "C" void set_plugin_callback(void (*cb)(void*, int), void* ctx);
```

You can still use Signal internally and bridge at the boundary. The internal code gets lifetime safety; only the edge requires C-style care:

```cpp
Signal<void(int)> internalSignal;

struct PluginState
{
    void (*mCb)(void*, int) = nullptr;
    void* mCtx = nullptr;
    ScopedConnection mConn;  // Must be stored to keep the subscription alive
};

extern "C" void set_plugin_callback(PluginState* state, void (*cb)(void*, int), void* ctx)
{
    state->mCb = cb;
    state->mCtx = ctx;

    // Store the connection in PluginState so it remains connected after this function returns.
    state->mConn = internalSignal.connect([state](int v) {
        if (state->mCb != nullptr)
        {
            state->mCb(state->mCtx, v);
        }
    });
}
```

### 2. Extreme Performance Requirements

Signal adds a layer of indirection. For most code, the overhead is negligible—a few nanoseconds per emission. But in truly hot paths (audio sample processing, high-frequency trading, inner physics loops), those nanoseconds add up:

```cpp
// Rough overhead comparison:
// Raw function pointer: ~1ns
// Signal with 1 listener: ~5-10ns
```

If profiling shows signal emission as a bottleneck, use a raw function pointer or `std::function` for that specific hot path. Don't prematurely optimize—measure first.

### 3. Return Value Aggregation Complexity

Signal's `emitCollect()` gathers return values into a vector. That's enough for most voting and validation patterns. But if you need weighted averages, quorum detection, or complex consensus logic, you'll write it yourself:

```cpp
auto results = sig.emitCollect(args);
auto winner = std::max_element(results.begin(), results.end());
```

Boost.Signals2's combiner system handles this more elegantly. If aggregation logic is central to your design, that might tip the balance toward Boost.

### 4. Very Large Number of Signals

Each Signal instance carries overhead—roughly 48 bytes minimum, more with connections. If your architecture has millions of tiny signals (one per particle, per pixel, per network packet), that memory adds up. A centralized event bus with topic routing might serve better.

### 5. You Need Boost.Signals2 Features

Boost.Signals2 has capabilities Fat-P Signal intentionally omits:

The `trackable` mixin automatically disconnects when an observed object is destroyed, without explicit connection management. If your codebase relies on this pattern, migrating to Fat-P requires adding `ScopedConnection` members everywhere—a non-trivial refactor.

Complex combiners aggregate return values with custom logic. Fat-P's `emitCollect()` is simpler but less powerful.

If these features are essential to your design, use Boost. Fat-P Signal chose simplicity and minimal dependencies over feature completeness.

---

## Summary

C-style callbacks work, but they accumulate technical debt: dangling pointers, manual lifetime tracking, reentrancy bugs, and no type safety. When you're ready to migrate, you have three options.

**std:: DIY** — No dependencies, but you build everything yourself. Good for simple cases where you don't need automatic disconnect or reentrancy safety.

**Boost.Signals2** — Mature and feature-complete. The right choice if you're already using Boost or need `trackable` auto-disconnect and return value combiners.

**Fat-P Signal** — Single header, no dependencies, fast compilation. Policy-based thread safety means you don't pay for what you don't use.

The examples in this guide show the same C code migrated all three ways. The tradeoffs are visible in the code itself.

---

## References

- [SQLite callback patterns](https://github.com/sqlite/sqlite/blob/master/src/main.c) — C-style callbacks in production
- [Boost.Signals2](https://www.boost.org/doc/libs/release/doc/html/signals2.html) — The mature alternative
- [std::function](https://en.cppreference.com/w/cpp/utility/functional/function) — Building block for DIY approach
- Fat-P User Manual: Signal — Complete API reference

---

*FAT-P Library Documentation — January 2025*
