# AtomicSharedPtr User Manual

## Table of Contents

1. [What is AtomicSharedPtr and Why Use It?](#what-is-atomicsharedptr-and-why-use-it)
   - [The Shared Pointer Concurrency Problem](#the-shared-pointer-concurrency-problem)
   - [The C++ Atomic Shared Pointer Landscape](#the-c-atomic-shared-pointer-landscape)
   - [Where AtomicSharedPtr Fits](#where-atomicsharedptr-fits)
2. [Core Architecture](#core-architecture)
   - [The Type System](#the-type-system)
   - [Version Detection](#version-detection)
   - [Design Decisions](#design-decisions)
3. [Getting Started](#getting-started)
   - [Prerequisites](#prerequisites)
   - [Integration](#integration)
   - [First Program](#first-program)
4. [Core Operations](#core-operations)
   - [Construction](#construction)
   - [Load Operations](#load-operations)
   - [Store Operations](#store-operations)
   - [Exchange Operations](#exchange-operations)
   - [Compare-and-Swap Operations](#compare-and-swap-operations)
5. [Null Enforcement](#null-enforcement)
   - [ThrowOnNull Parameter](#throwonnull-parameter)
   - [raw_load Escape Hatch](#raw_load-escape-hatch)
   - [When to Use Each](#when-to-use-each)
6. [Memory Ordering](#memory-ordering)
   - [What is Memory Ordering?](#what-is-memory-ordering)
   - [Default Orderings](#default-orderings)
   - [Custom Orderings](#custom-orderings)
   - [Guidelines](#guidelines)
7. [Wait and Notify (C++20)](#wait-and-notify-c20)
   - [What is Wait/Notify?](#what-is-waitnotify)
   - [Basic Usage](#basic-usage)
   - [Why C++20 Only](#why-c20-only)
8. [Factory and Traits](#factory-and-traits)
   - [make_atomic_shared_ptr](#make_atomic_shared_ptr)
   - [is_atomic_shared_ptr](#is_atomic_shared_ptr)
9. [Thread Safety Guarantees](#thread-safety-guarantees)
   - [What is Thread-Safe](#what-is-thread-safe)
   - [What is NOT Thread-Safe](#what-is-not-thread-safe)
   - [The "Atomic Handle" Concept](#the-atomic-handle-concept)
10. [Common Patterns](#common-patterns)
    - [Global Configuration](#global-configuration)
    - [Lazy Initialization](#lazy-initialization)
    - [Read-Copy-Update](#read-copy-update)
    - [Producer-Consumer](#producer-consumer)
11. [Performance Characteristics](#performance-characteristics)
    - [Lock-Free Status](#lock-free-status)
    - [Overhead Analysis](#overhead-analysis)
    - [Benchmarking Tips](#benchmarking-tips)
12. [Comparison with Alternatives](#comparison-with-alternatives)
    - [vs std::atomic<shared_ptr>](#vs-stdatomicshared_ptr)
    - [vs Free Atomic Functions](#vs-free-atomic-functions)
    - [vs Mutex + shared_ptr](#vs-mutex--shared_ptr)
    - [vs The Previous AtomicReference](#vs-the-previous-atomicreference)
13. [Migration Guide](#migration-guide)
    - [From std::atomic<shared_ptr>](#from-stdatomicshared_ptr)
    - [From Free Atomic Functions](#from-free-atomic-functions)
    - [From Mutex + shared_ptr](#from-mutex--shared_ptr)
14. [Best Practices](#best-practices)
    - [Design Patterns](#design-patterns)
    - [Error Handling](#error-handling)
    - [Testing](#testing)
15. [Troubleshooting](#troubleshooting)
    - [Compilation Errors](#compilation-errors)
    - [Runtime Issues](#runtime-issues)
    - [Common Mistakes](#common-mistakes)
16. [Known Limitations](#known-limitations)
17. [API Reference](#api-reference)
18. [Summary](#summary)

---

## What is AtomicSharedPtr and Why Use It?

### The Shared Pointer Concurrency Problem

`std::shared_ptr` is C++'s answer to automatic memory management—reference counting that deletes objects when the last owner goes away. It works beautifully in single-threaded code:

```cpp
std::shared_ptr<Config> config = std::make_shared<Config>(8080, "localhost");
auto copy = config;  // Refcount: 2
config.reset();      // Refcount: 1
// copy still valid, config object still alive
```

But what happens when multiple threads access the same `shared_ptr` variable?

```cpp
std::shared_ptr<Config> global_config;  // Shared between threads

// Thread 1
auto local = global_config;  // Read the pointer

// Thread 2
global_config = std::make_shared<Config>(9090, "remote");  // Write the pointer

// UNDEFINED BEHAVIOR: data race on the shared_ptr itself
```

The reference counting inside `shared_ptr` is thread-safe—you can safely copy a `shared_ptr` while another thread copies the same `shared_ptr`. But the `shared_ptr` *variable itself* is not thread-safe. Reading `global_config` while another thread writes to it is a data race.

This distinction confuses many programmers. The *object* pointed to by the `shared_ptr` and the *copies* of the `shared_ptr` are safe to use concurrently. But the *variable* holding a `shared_ptr` requires synchronization if multiple threads read and write it.

### The C++ Atomic Shared Pointer Landscape

C++ provides two solutions to this problem:

**C++11/14/17: Free atomic functions**

```cpp
std::shared_ptr<Config> global_config;

// Thread-safe read
auto local = std::atomic_load(&global_config);

// Thread-safe write
std::atomic_store(&global_config, std::make_shared<Config>(9090, "remote"));

// Thread-safe compare-and-swap
std::shared_ptr<Config> expected = local;
std::atomic_compare_exchange_strong(&global_config, &expected, new_config);
```

These functions work, but they're verbose, easy to forget, and don't compose well.

**C++20: std::atomic<std::shared_ptr<T>>**

```cpp
std::atomic<std::shared_ptr<Config>> global_config;

// Clean syntax
auto local = global_config.load();
global_config.store(std::make_shared<Config>(9090, "remote"));

// Plus wait/notify
global_config.wait(old_value);
global_config.notify_all();
```

This is the modern solution, but it requires C++20, which many codebases can't adopt yet.

### Where AtomicSharedPtr Fits

AtomicSharedPtr provides a single API that works on both C++17 and C++20:

```cpp
fat_p::AtomicSharedPtr<Config> global_config;

// Same code works on C++17 and C++20
auto local = global_config.load();
global_config.store(std::make_shared<Config>(9090, "remote"));

// Optional null enforcement
fat_p::AtomicSharedPtr<Config, true> safe_config;
auto ptr = safe_config.load();  // Throws if null
```

**What AtomicSharedPtr provides:**

- Unified API across C++17 and C++20
- Optional null-checking with zero cost when disabled
- Sensible default memory orderings
- Factory function and type traits
- 196 lines of focused, auditable code

**What AtomicSharedPtr intentionally omits:**

- weak_ptr support (use `std::mutex` + `std::weak_ptr` if needed)
- Custom wait policies (use native `wait()` on C++20 or write your own)
- Complex policy hierarchies (one boolean parameter is enough)

---

## Core Architecture

### The Type System

AtomicSharedPtr is a class template with two parameters:

```cpp
template <typename T, bool ThrowOnNull = false>
class AtomicSharedPtr;
```

| Parameter | Purpose | Default |
|-----------|---------|---------|
| `T` | The type managed by the shared_ptr | Required |
| `ThrowOnNull` | Whether `load()` throws on null | `false` |

The internal storage adapts to your C++ version:

```cpp
#if FATP_HAS_CPP20_ATOMIC_SHARED_PTR
    std::atomic<std::shared_ptr<T>> ptr_;  // Native C++20
#else
    std::shared_ptr<T> ptr_;  // Uses free atomic functions
#endif
```

### Version Detection

The macro `FATP_HAS_CPP20_ATOMIC_SHARED_PTR` detects C++20 `std::atomic<shared_ptr>` support:

```cpp
#if defined(__cpp_lib_atomic_shared_ptr) && __cpp_lib_atomic_shared_ptr >= 201711L
#define FATP_HAS_CPP20_ATOMIC_SHARED_PTR 1
#else
#define FATP_HAS_CPP20_ATOMIC_SHARED_PTR 0
#endif
```

This uses the standard library feature test macro `__cpp_lib_atomic_shared_ptr` rather than checking `__cplusplus`. This is more accurate—a compiler might support C++20 syntax but the standard library might not have `std::atomic<shared_ptr>` yet. The feature test macro is authoritative.

### Design Decisions

**Why a boolean instead of policy classes?**

The previous AtomicReference had four enforcement policies:

```cpp
// Old approach: 4 policies, dense template parameters
AtomicReference<Config, DebugOnlyPolicy, PollingWaitPolicy, std::chrono::seconds>
```

In practice, users wanted two behaviors: "throw on null" or "don't throw on null." The policy hierarchy added complexity without value. A boolean is simpler:

```cpp
// New approach: clear intent
AtomicSharedPtr<Config>        // Nullable
AtomicSharedPtr<Config, true>  // Throws on null
```

**Why no weak_ptr support?**

`std::atomic<std::weak_ptr<T>>` doesn't exist in the standard. The previous AtomicReference emulated it with a mutex, but:

- A mutex-based "atomic" is misleading—it's not lock-free
- The API suggested capabilities it didn't have
- The implementation was the source of most bugs

If you need thread-safe weak_ptr access, be explicit:

```cpp
std::mutex mtx;
std::weak_ptr<T> wp;

// Explicit locking—no illusions
std::lock_guard lock(mtx);
auto sp = wp.lock();
```

**Why no custom wait policies?**

The previous AtomicReference had `PollingWaitPolicy`, `NativeWaitPolicy`, and `WaitPolicyPack`. These reimplemented what `std::atomic::wait` already does, with custom spin counts and yield intervals that were tuned for... nobody's hardware in particular.

On C++20, use native `wait()`—it's tuned by compiler and OS vendors for your actual platform. On C++17, if you need blocking wait semantics, write a condition variable wrapper for your specific use case.

---

## Getting Started

### Prerequisites

| Requirement | Specification |
|-------------|---------------|
| C++ Standard | C++17 or later |
| Compiler | GCC 7+, Clang 5+, MSVC 2017+ |
| Dependencies | None (standard library only) |

### Integration

AtomicSharedPtr is a single header file. Copy it to your project:

```
your_project/
├── include/
│   └── AtomicSharedPtr.h
├── src/
│   └── main.cpp
```

Include it:

```cpp
#include "AtomicSharedPtr.h"
```

### First Program

```cpp
#include <iostream>
#include <thread>
#include "AtomicSharedPtr.h"

struct Config
{
    int port;
    std::string host;
};

int main()
{
    // Create an atomic shared pointer
    fat_p::AtomicSharedPtr<Config> config(
        std::make_shared<Config>(Config{8080, "localhost"})
    );

    // Reader thread
    std::thread reader([&]()
    {
        for (int i = 0; i < 100; ++i)
        {
            auto cfg = config.load();
            std::cout << "Port: " << cfg->port << "\n";
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });

    // Writer thread
    std::thread writer([&]()
    {
        for (int i = 0; i < 10; ++i)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            config.store(std::make_shared<Config>(Config{8080 + i, "localhost"}));
        }
    });

    reader.join();
    writer.join();

    std::cout << "Final port: " << config.load()->port << "\n";
    return 0;
}
```

Compile:

```bash
# C++17
g++ -std=c++17 -pthread main.cpp -o main

# C++20 (enables wait/notify)
g++ -std=c++20 -pthread main.cpp -o main
```

---

## Core Operations

### Construction

```cpp
// Default: null pointer
fat_p::AtomicSharedPtr<Config> ref1;
assert(!ref1.raw_load());

// From existing shared_ptr
auto sp = std::make_shared<Config>(8080, "localhost");
fat_p::AtomicSharedPtr<Config> ref2(sp);
assert(ref2.load()->port == 8080);

// Using factory function
auto ref3 = fat_p::make_atomic_shared_ptr<Config>(8080, "localhost");
assert(ref3.load()->port == 8080);
```

**Not copyable or movable:**

```cpp
fat_p::AtomicSharedPtr<Config> ref1;
// fat_p::AtomicSharedPtr<Config> ref2 = ref1;  // Compilation error
// fat_p::AtomicSharedPtr<Config> ref3 = std::move(ref1);  // Compilation error
```

This is intentional. Copying an atomic would have confusing semantics—should it atomically read and copy, or copy the atomic wrapper? Deleting copy/move forces explicit `load()` and construction.

### Load Operations

**load()**: Atomically reads the current value.

```cpp
fat_p::AtomicSharedPtr<Config> ref(std::make_shared<Config>(8080, "localhost"));

std::shared_ptr<Config> ptr = ref.load();
std::cout << ptr->port << "\n";  // 8080
```

With `ThrowOnNull = true`, throws if null:

```cpp
fat_p::AtomicSharedPtr<Config, true> ref;  // Null
// auto ptr = ref.load();  // Throws std::runtime_error
```

**raw_load()**: Always returns the pointer without throwing, even with `ThrowOnNull = true`.

```cpp
fat_p::AtomicSharedPtr<Config, true> ref;  // Null
auto ptr = ref.raw_load();  // Returns nullptr, no exception
if (ptr)
{
    // Use ptr
}
```

### Store Operations

**store()**: Atomically replaces the current value.

```cpp
fat_p::AtomicSharedPtr<Config> ref;

ref.store(std::make_shared<Config>(8080, "localhost"));
assert(ref.load()->port == 8080);

ref.store(std::make_shared<Config>(9090, "remote"));
assert(ref.load()->port == 9090);

ref.store(nullptr);  // Valid: sets to null
assert(!ref.raw_load());
```

**Memory ordering**: Default is `memory_order_release`, which ensures all prior writes are visible to threads that subsequently `load()` with `memory_order_acquire`.

### Exchange Operations

**exchange()**: Atomically replaces the value and returns the old value.

```cpp
fat_p::AtomicSharedPtr<Config> ref(std::make_shared<Config>(8080, "localhost"));

auto old = ref.exchange(std::make_shared<Config>(9090, "remote"));

assert(old->port == 8080);           // Old value
assert(ref.load()->port == 9090);    // New value
```

This is useful for "take ownership" patterns:

```cpp
// Thread 1: Set new config and process old one
auto old_config = ref.exchange(new_config);
cleanup(old_config);  // Safe: we have exclusive ownership of old

// Thread 2 and beyond see new_config immediately
```

### Compare-and-Swap Operations

**compare_exchange_weak()** and **compare_exchange_strong()**: Atomically replace the value only if it matches an expected value.

```cpp
fat_p::AtomicSharedPtr<Config> ref(std::make_shared<Config>(8080, "localhost"));

auto expected = ref.load();
auto desired = std::make_shared<Config>(expected->port + 1, expected->host);

if (ref.compare_exchange_strong(expected, desired))
{
    std::cout << "Updated port to " << ref.load()->port << "\n";
}
else
{
    std::cout << "Another thread changed it first\n";
    std::cout << "Current port: " << expected->port << "\n";  // expected was updated
}
```

**Weak vs Strong:**

- `compare_exchange_weak` may fail spuriously even when values match—use in loops
- `compare_exchange_strong` only fails when values truly differ—use for single attempts

```cpp
// Typical CAS loop pattern using weak
auto expected = ref.load();
while (true)
{
    auto desired = compute_new_value(expected);
    if (ref.compare_exchange_weak(expected, desired))
    {
        break;  // Success
    }
    // expected was updated to current value; loop and retry
}
```

---

## Null Enforcement

### ThrowOnNull Parameter

The second template parameter controls null-checking behavior:

```cpp
// Default: nullable (no checking)
fat_p::AtomicSharedPtr<Config> nullable_ref;
auto ptr = nullable_ref.load();  // Returns nullptr

// ThrowOnNull: throws on null load
fat_p::AtomicSharedPtr<Config, true> enforced_ref;
// auto ptr = enforced_ref.load();  // Throws std::runtime_error
```

**Exception message:**

```
AtomicSharedPtr::load() returned null
```

### raw_load Escape Hatch

Even with `ThrowOnNull = true`, you sometimes need to check for null without throwing:

```cpp
fat_p::AtomicSharedPtr<Config, true> ref;

// Check without throwing
if (ref.raw_load())
{
    auto ptr = ref.load();  // Safe: we know it's not null
    use(ptr);
}
else
{
    // Handle null case
}
```

### When to Use Each

| Scenario | Recommendation |
|----------|----------------|
| Pointer may legitimately be null | `AtomicSharedPtr<T>` (default) |
| Null indicates a bug | `AtomicSharedPtr<T, true>` |
| Conditional check needed | Use `raw_load()` |
| Performance-critical path | `raw_load()` avoids branch |

---

## Memory Ordering

### What is Memory Ordering?

Modern CPUs and compilers reorder instructions for performance. Memory ordering constraints tell them what reorderings are allowed.

```cpp
// Without ordering constraints, this could execute out of order:
data = 42;        // Write A
ready.store(true);  // Write B
// CPU might execute B before A!
```

Memory orderings prevent dangerous reorderings:

| Ordering | Meaning |
|----------|---------|
| `relaxed` | No ordering constraints (fastest) |
| `acquire` | Subsequent reads/writes cannot move before this |
| `release` | Prior reads/writes cannot move after this |
| `acq_rel` | Both acquire and release |
| `seq_cst` | Total ordering (safest, slowest) |

### Default Orderings

AtomicSharedPtr uses safe defaults:

| Operation | Default Ordering | Rationale |
|-----------|------------------|-----------|
| `load()` | `acquire` | See writes that happened before the store |
| `store()` | `release` | Make prior writes visible to loaders |
| `exchange()` | `acq_rel` | Both read and write |
| `compare_exchange_*` | `acq_rel` / `acquire` | Success uses acq_rel, failure uses acquire |

These defaults are correct for typical producer-consumer patterns.

### Custom Orderings

All operations accept optional memory order parameters:

```cpp
// Relaxed load for statistics (no synchronization needed)
auto ptr = ref.load(std::memory_order_relaxed);

// Sequential consistency for lock-free algorithms
ref.store(new_ptr, std::memory_order_seq_cst);

// Custom CAS orderings
ref.compare_exchange_strong(expected, desired,
    std::memory_order_acq_rel,    // Success ordering
    std::memory_order_relaxed);   // Failure ordering
```

### Guidelines

| Use Case | Recommended Ordering |
|----------|----------------------|
| Don't know what you need | Use defaults |
| Counter/statistics | `relaxed` |
| Flag/ready signal | `release` for writer, `acquire` for reader |
| Lock-free data structures | `acq_rel` or `seq_cst` |

**When in doubt, use the defaults.** Incorrect memory ordering causes bugs that are nearly impossible to reproduce and debug.

---

## Wait and Notify (C++20)

### What is Wait/Notify?

Wait/notify provides efficient blocking until a value changes:

```cpp
// Without wait: spin loop (wastes CPU)
while (ref.load() == old_value)
{
    std::this_thread::yield();
}

// With wait: efficient blocking
ref.wait(old_value);  // Blocks until ref != old_value
```

The OS suspends the waiting thread and wakes it only when another thread calls `notify_one()` or `notify_all()`.

### Basic Usage

```cpp
#if FATP_HAS_CPP20_ATOMIC_SHARED_PTR

fat_p::AtomicSharedPtr<Config> config(initial);

// Consumer thread
void consumer()
{
    auto old = config.load();
    while (running)
    {
        config.wait(old);  // Block until changed
        old = config.load();
        process(old);
    }
}

// Producer thread
void producer()
{
    while (running)
    {
        auto new_config = compute_new_config();
        config.store(new_config);
        config.notify_all();  // Wake all waiters
    }
}

#endif
```

**notify_one() vs notify_all():**

- `notify_one()`: Wakes one waiting thread (more efficient if only one consumer)
- `notify_all()`: Wakes all waiting threads (use when multiple consumers need the update)

### Why C++20 Only

Wait/notify is only available when `FATP_HAS_CPP20_ATOMIC_SHARED_PTR` is true. On C++17, these methods don't exist:

```cpp
#if FATP_HAS_CPP20_ATOMIC_SHARED_PTR
    ref.wait(old);
    ref.notify_all();
#else
    // Must implement your own wait mechanism
    std::mutex mtx;
    std::condition_variable cv;
    // ...
#endif
```

**Why no C++17 fallback?**

A polling-based `wait()` would be inefficient and misleading. Users would expect `wait()` to block efficiently, but it would actually spin. Better to make the absence explicit so users can make informed decisions about their C++17 code.

---

## Factory and Traits

### make_atomic_shared_ptr

Creates an AtomicSharedPtr with a newly constructed object:

```cpp
// Basic usage
auto ref = fat_p::make_atomic_shared_ptr<Config>(8080, "localhost");

// With ThrowOnNull
auto safe_ref = fat_p::make_atomic_shared_ptr<Config, true>(8080, "localhost");

// Equivalent to:
fat_p::AtomicSharedPtr<Config> ref(std::make_shared<Config>(8080, "localhost"));
```

Arguments are forwarded to the `Config` constructor.

### is_atomic_shared_ptr

Type trait for detecting AtomicSharedPtr:

```cpp
static_assert(fat_p::is_atomic_shared_ptr_v<fat_p::AtomicSharedPtr<int>>);
static_assert(fat_p::is_atomic_shared_ptr_v<fat_p::AtomicSharedPtr<int, true>>);
static_assert(!fat_p::is_atomic_shared_ptr_v<std::shared_ptr<int>>);
static_assert(!fat_p::is_atomic_shared_ptr_v<int>);
```

Useful in templates:

```cpp
template <typename T>
void process(T& ref)
{
    if constexpr (fat_p::is_atomic_shared_ptr_v<T>)
    {
        auto ptr = ref.load();
        // ...
    }
    else
    {
        // Handle other types
    }
}
```

---

## Thread Safety Guarantees

### What is Thread-Safe

The following operations can be called concurrently from multiple threads without external synchronization:

| Operation | Thread-Safe | Notes |
|-----------|-------------|-------|
| `load()` | ✓ | Multiple concurrent loads are safe |
| `raw_load()` | ✓ | Same as load() |
| `store()` | ✓ | Multiple concurrent stores are safe |
| `exchange()` | ✓ | Safe with concurrent loads/stores |
| `compare_exchange_*` | ✓ | Safe with concurrent loads/stores |
| `wait()` | ✓ | Can wait while others load/store |
| `notify_*()` | ✓ | Can notify while others wait |
| `is_lock_free()` | ✓ | Read-only query |
| `operator bool()` | ✓ | Calls raw_load() internally |

### What is NOT Thread-Safe

**Accessing the managed object:**

```cpp
fat_p::AtomicSharedPtr<std::vector<int>> ref(std::make_shared<std::vector<int>>());

// Thread 1
ref.load()->push_back(1);  // NOT THREAD-SAFE

// Thread 2
ref.load()->push_back(2);  // DATA RACE with Thread 1
```

The AtomicSharedPtr provides atomic access to *which* vector you're pointing to, not atomic access to *the vector itself*.

**Chained operations:**

```cpp
// NOT ATOMIC as a whole
if (ref.load())
{
    // Another thread could store(nullptr) here!
    ref.load()->doSomething();  // Potential null dereference
}

// SAFE: single load, then use the local copy
auto ptr = ref.load();
if (ptr)
{
    ptr->doSomething();  // Safe: ptr is a local copy
}
```

### The "Atomic Handle" Concept

Think of AtomicSharedPtr as an atomic *handle* to an object, not an atomic *object*:

```
┌─────────────────────────────────────────────────────────────┐
│                     AtomicSharedPtr                         │
│  ┌─────────────────────────────────────────────────────┐    │
│  │  Atomic operations on the pointer/control block     │    │
│  │  • load() - atomic read of which object             │    │
│  │  • store() - atomic write of which object           │    │
│  │  • CAS - atomic conditional swap                    │    │
│  └─────────────────────────────────────────────────────┘    │
│                           │                                  │
│                           ▼                                  │
│  ┌─────────────────────────────────────────────────────┐    │
│  │  The managed object T                               │    │
│  │  • NOT protected by AtomicSharedPtr                 │    │
│  │  • Needs its own synchronization if shared         │    │
│  └─────────────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────────────┘
```

---

## Common Patterns

### Global Configuration

```cpp
// Global config accessible from any thread
fat_p::AtomicSharedPtr<Config> g_config;

void initialize()
{
    g_config.store(std::make_shared<Config>(load_from_file("config.json")));
}

Config get_config()
{
    auto ptr = g_config.load();
    if (!ptr)
    {
        throw std::runtime_error("Config not initialized");
    }
    return *ptr;  // Return copy
}

void reload_config()
{
    g_config.store(std::make_shared<Config>(load_from_file("config.json")));
    // All threads see new config on next load()
}
```

### Lazy Initialization

```cpp
fat_p::AtomicSharedPtr<ExpensiveResource> g_resource;

std::shared_ptr<ExpensiveResource> get_resource()
{
    auto ptr = g_resource.load();
    if (ptr)
    {
        return ptr;
    }

    // Double-checked locking pattern
    static std::mutex init_mutex;
    std::lock_guard lock(init_mutex);

    ptr = g_resource.load();  // Check again under lock
    if (!ptr)
    {
        ptr = std::make_shared<ExpensiveResource>();
        g_resource.store(ptr);
    }
    return ptr;
}
```

### Read-Copy-Update

```cpp
fat_p::AtomicSharedPtr<std::map<std::string, int>> g_data;

// Reader: no locking needed
int read(const std::string& key)
{
    auto data = g_data.load();
    auto it = data->find(key);
    return it != data->end() ? it->second : -1;
}

// Writer: copy, modify, swap
void write(const std::string& key, int value)
{
    auto expected = g_data.load();
    while (true)
    {
        // Copy the current data
        auto new_data = std::make_shared<std::map<std::string, int>>(*expected);
        (*new_data)[key] = value;

        // Try to swap in the new version
        if (g_data.compare_exchange_weak(expected, new_data))
        {
            break;  // Success
        }
        // expected was updated; loop and retry with new base
    }
}
```

### Producer-Consumer

```cpp
#if FATP_HAS_CPP20_ATOMIC_SHARED_PTR

fat_p::AtomicSharedPtr<WorkItem> g_work;
std::atomic<bool> g_running{true};

void producer()
{
    while (g_running)
    {
        auto work = generate_work();
        g_work.store(work);
        g_work.notify_one();
    }
}

void consumer()
{
    std::shared_ptr<WorkItem> last_seen;
    while (g_running)
    {
        g_work.wait(last_seen);
        last_seen = g_work.load();
        if (last_seen)
        {
            process(last_seen);
        }
    }
}

#endif
```

---

## Performance Characteristics

### Lock-Free Status

Most implementations of `std::atomic<std::shared_ptr>` are **not lock-free**:

```cpp
fat_p::AtomicSharedPtr<int> ref;

// Runtime query
std::cout << "Lock-free: " << ref.is_lock_free() << "\n";
// Typically prints: Lock-free: 0

// Compile-time query
constexpr bool always_lf = fat_p::AtomicSharedPtr<int>::is_always_lock_free();
static_assert(!always_lf, "shared_ptr atomics are rarely lock-free");
```

**Why two functions?**

| Function | When to Use |
|----------|-------------|
| `is_lock_free()` | Runtime check; on C++20, queries the actual implementation |
| `is_always_lock_free()` | Compile-time constant; safe for `static_assert` and `if constexpr` |

**C++17 behavior:** Both functions return `false`. The free-function shared_ptr atomics (`std::atomic_load`, etc.) typically use a global lock table, and there's no standard way to query their lock-free status. We conservatively report `false`.

**C++20 behavior:** Queries the actual `std::atomic<shared_ptr>` implementation, which may vary by platform.

**Why not lock-free?** Atomic operations on `shared_ptr` must update both the pointer and the reference count atomically. This requires either a double-width CAS instruction (not available on all architectures) or internal locking (what most implementations do).

**Practical impact:** For most applications, this doesn't matter. The internal locks are highly optimized and contention is rare in typical use.

### Overhead Analysis

| Operation | Overhead vs Raw | Notes |
|-----------|-----------------|-------|
| Construction | ~0 | Same as shared_ptr construction |
| load() | ~0 | Inlines to atomic load |
| load() with ThrowOnNull | +1 branch | Null check |
| store() | ~0 | Inlines to atomic store |
| exchange() | ~0 | Inlines to atomic exchange |
| CAS | ~0 | Inlines to atomic CAS |

The wrapper adds no runtime overhead. All operations inline to their underlying implementations.

### Benchmarking Tips

```cpp
#include <chrono>

void benchmark()
{
    fat_p::AtomicSharedPtr<int> ref(std::make_shared<int>(42));
    constexpr int iterations = 10'000'000;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < iterations; ++i)
    {
        auto ptr = ref.load();
        // DoNotOptimize(ptr);  // If using benchmark library
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

    std::cout << "load(): " << (ns / iterations) << " ns/op\n";
}
```

**Important:** Always benchmark with optimizations enabled (`-O2` or `/O2`).

---

## Comparison with Alternatives

### vs std::atomic<shared_ptr>

| Aspect | AtomicSharedPtr | std::atomic<shared_ptr> |
|--------|-----------------|-------------------------|
| C++ version | C++17+ | C++20+ only |
| Null enforcement | Optional `ThrowOnNull` | None |
| Default orderings | Safe defaults | Must specify |
| API | Member functions | Member functions |
| Factory | `make_atomic_shared_ptr` | None |
| Type traits | `is_atomic_shared_ptr_v` | None |

**Choose AtomicSharedPtr when:** You need C++17 support, want null enforcement, or prefer the convenience API.

**Choose std::atomic<shared_ptr> when:** You're on C++20, need maximum standards compliance, or want to avoid any wrapper.

### vs Free Atomic Functions

| Aspect | AtomicSharedPtr | Free Functions |
|--------|-----------------|----------------|
| Syntax | `ref.load()` | `std::atomic_load(&ptr)` |
| Null enforcement | Optional | None |
| Discoverability | IDE autocomplete works | Must remember function names |
| Error-proneness | Low | Easy to forget `&` |

**Choose AtomicSharedPtr when:** You want cleaner syntax and fewer mistakes.

**Choose free functions when:** You can't add a dependency (but AtomicSharedPtr is a single header).

### vs Mutex + shared_ptr

| Aspect | AtomicSharedPtr | Mutex + shared_ptr |
|--------|-----------------|-------------------|
| Performance | Better under low contention | Better under high contention |
| Complexity | Simple | Requires careful lock management |
| Deadlock risk | None | Possible |
| Read scalability | Excellent | Limited by mutex |

**Choose AtomicSharedPtr when:** Reads greatly outnumber writes, or contention is low.

**Choose mutex when:** You need to protect multiple operations atomically, or contention is high.

### vs The Previous AtomicReference

| Aspect | AtomicSharedPtr | AtomicReference (old) |
|--------|-----------------|----------------------|
| Lines of code | 196 | 1,905 |
| Template parameters | 2 | 4 |
| weak_ptr support | No | Yes (mutex-based) |
| Wait policies | Native only (C++20) | Custom policies |
| Bugs found in reviews | 0 | 12+ |

**Choose AtomicSharedPtr:** Always. The old AtomicReference is deprecated.

---

## Migration Guide

### From std::atomic<shared_ptr>

```cpp
// Before (C++20)
std::atomic<std::shared_ptr<Config>> config;
auto ptr = config.load();
config.store(new_config);

// After
fat_p::AtomicSharedPtr<Config> config;
auto ptr = config.load();      // Identical
config.store(new_config);      // Identical
```

The API is intentionally compatible. Migration is mechanical.

### From Free Atomic Functions

```cpp
// Before (C++17)
std::shared_ptr<Config> config;
auto ptr = std::atomic_load(&config);
std::atomic_store(&config, new_config);
std::atomic_compare_exchange_strong(&config, &expected, desired);

// After
fat_p::AtomicSharedPtr<Config> config;
auto ptr = config.load();
config.store(new_config);
config.compare_exchange_strong(expected, desired);
```

### From Mutex + shared_ptr

```cpp
// Before
std::mutex mtx;
std::shared_ptr<Config> config;

auto get_config() {
    std::lock_guard lock(mtx);
    return config;
}

void set_config(std::shared_ptr<Config> new_config) {
    std::lock_guard lock(mtx);
    config = new_config;
}

// After
fat_p::AtomicSharedPtr<Config> config;

auto get_config() {
    return config.load();
}

void set_config(std::shared_ptr<Config> new_config) {
    config.store(new_config);
}
```

**Note:** If your mutex protected multiple operations, you may need to restructure:

```cpp
// This pattern requires mutex—can't migrate directly
std::lock_guard lock(mtx);
if (!config) {
    config = create_default();
}
return config;

// AtomicSharedPtr alternative: CAS loop
auto ptr = config.load();
if (!ptr) {
    auto new_config = create_default();
    config.compare_exchange_strong(ptr, new_config);
    ptr = config.load();
}
return ptr;
```

---

## Best Practices

### Design Patterns

**Prefer immutable objects:**

```cpp
// Good: immutable Config, swap entire object
struct Config {
    const int port;
    const std::string host;
};
config.store(std::make_shared<Config>(9090, "remote"));

// Avoid: mutable Config with atomic pointer
config.load()->port = 9090;  // NOT THREAD-SAFE
```

**Use local copies:**

```cpp
// Good: load once, use local copy
auto cfg = config.load();
use(cfg->port);
use(cfg->host);
use(cfg->timeout);

// Avoid: multiple loads
use(config.load()->port);
use(config.load()->host);   // Might be different config!
use(config.load()->timeout);
```

**Initialize early:**

```cpp
// Good: initialize before spawning threads
config.store(load_initial_config());
std::thread worker1(run);
std::thread worker2(run);

// Avoid: initialize from worker
std::thread worker1([&]{ if (!config.load()) config.store(...); });  // Race!
```

### Error Handling

**Use ThrowOnNull for invariants:**

```cpp
// Config must always exist after initialization
fat_p::AtomicSharedPtr<Config, true> g_config;

void initialize() {
    g_config.store(std::make_shared<Config>());
}

// Any load() after initialize() is guaranteed non-null
// A null load indicates a bug (forgot to initialize)
```

**Use raw_load() for conditional checks:**

```cpp
fat_p::AtomicSharedPtr<Cache, true> g_cache;

void maybe_use_cache() {
    if (auto cache = g_cache.raw_load()) {
        // Use cache
    }
    // No exception if cache not initialized
}
```

### Testing

**Test concurrent access:**

```cpp
TEST_CASE(concurrent_load_store)
{
    fat_p::AtomicSharedPtr<int> ref(std::make_shared<int>(0));
    std::atomic<bool> running{true};
    std::atomic<int> reads{0};

    std::thread reader([&]() {
        while (running) {
            auto ptr = ref.load();
            SIMPLE_ASSERT(ptr != nullptr, "Should never be null");
            reads++;
        }
    });

    for (int i = 1; i <= 1000; ++i) {
        ref.store(std::make_shared<int>(i));
    }

    running = false;
    reader.join();

    SIMPLE_ASSERT(reads > 0, "Reader should have run");
    return true;
}
```

---

## Troubleshooting

### Compilation Errors

**"no member named 'wait'":**

```cpp
ref.wait(old);  // Error on C++17
```

`wait()` and `notify_*()` are only available on C++20. Check with:

```cpp
#if FATP_HAS_CPP20_ATOMIC_SHARED_PTR
    ref.wait(old);
#else
    // Alternative implementation
#endif
```

**"deleted function 'AtomicSharedPtr(const AtomicSharedPtr&)'":**

```cpp
auto ref2 = ref1;  // Error: copy deleted
```

AtomicSharedPtr is not copyable. Use `load()` to get the underlying pointer:

```cpp
auto ptr = ref1.load();
fat_p::AtomicSharedPtr<T> ref2(ptr);
```

**"cannot convert 'T*' to 'std::shared_ptr<T>'":**

```cpp
ref.store(new Config());  // Error: raw pointer
```

Use `std::make_shared`:

```cpp
ref.store(std::make_shared<Config>());
```

### Runtime Issues

**Null pointer dereference with ThrowOnNull=false:**

```cpp
auto ptr = ref.load();  // Returns nullptr
ptr->method();          // CRASH
```

Always check before dereferencing:

```cpp
auto ptr = ref.load();
if (ptr) {
    ptr->method();
}
```

Or use `ThrowOnNull=true` and catch exceptions.

**Data race on managed object:**

```cpp
ref.load()->data.push_back(x);  // Thread 1
ref.load()->data.push_back(y);  // Thread 2 — DATA RACE
```

AtomicSharedPtr protects the pointer, not the object. Use immutable objects or add synchronization inside the managed type.

### Common Mistakes

**Assuming atomicity of multiple operations:**

```cpp
// NOT ATOMIC
if (ref.load() != nullptr) {
    ref.load()->use();  // Another thread could store(nullptr) between lines
}

// CORRECT
auto ptr = ref.load();
if (ptr) {
    ptr->use();
}
```

**Forgetting CAS can fail:**

```cpp
auto expected = ref.load();
ref.compare_exchange_strong(expected, desired);
use(expected);  // BUG: expected might have been updated!

// CORRECT: check return value
if (ref.compare_exchange_strong(expected, desired)) {
    // Success: desired is now stored
} else {
    // Failure: expected contains current value
}
```

---

## Known Limitations

| Limitation | Impact | Workaround |
|------------|--------|------------|
| No weak_ptr support | Can't atomically access weak_ptr | Use `std::mutex` + `std::weak_ptr` |
| No wait() on C++17 | Can't block efficiently | Use condition_variable or polling |
| Usually not lock-free | May have internal locks | Rarely matters in practice |
| No ABA protection | Theoretical issue for certain patterns | Use full CAS loop with version |

---

## API Reference

### Class Template

```cpp
template <typename T, bool ThrowOnNull = false>
class AtomicSharedPtr;
```

### Type Aliases

```cpp
using value_type = T;
using pointer_type = std::shared_ptr<T>;
```

### Constructors

| Signature | Description |
|-----------|-------------|
| `AtomicSharedPtr() noexcept` | Default: null pointer |
| `explicit AtomicSharedPtr(std::shared_ptr<T> p) noexcept` | From shared_ptr |
| `AtomicSharedPtr(const AtomicSharedPtr&) = delete` | Not copyable |
| `AtomicSharedPtr(AtomicSharedPtr&&) = delete` | Not movable |

### Member Functions

| Signature | Description |
|-----------|-------------|
| `std::shared_ptr<T> load(memory_order = acquire) const` | Atomic load; throws if null and ThrowOnNull |
| `std::shared_ptr<T> raw_load(memory_order = acquire) const noexcept` | Atomic load; never throws |
| `void store(std::shared_ptr<T>, memory_order = release) noexcept` | Atomic store |
| `std::shared_ptr<T> exchange(std::shared_ptr<T>, memory_order = acq_rel) noexcept` | Atomic exchange |
| `bool compare_exchange_weak(shared_ptr<T>&, shared_ptr<T>, memory_order, memory_order) noexcept` | Weak CAS |
| `bool compare_exchange_strong(shared_ptr<T>&, shared_ptr<T>, memory_order, memory_order) noexcept` | Strong CAS |
| `void wait(std::shared_ptr<T>, memory_order = acquire) const` | Block until changed (C++20) |
| `void notify_one() noexcept` | Wake one waiter (C++20) |
| `void notify_all() noexcept` | Wake all waiters (C++20) |
| `bool is_lock_free() const noexcept` | Runtime lock-free query (C++17: always false) |
| `static constexpr bool is_always_lock_free() noexcept` | Compile-time lock-free query (C++17: always false) |
| `explicit operator bool() const noexcept` | True if non-null |

### Free Functions

```cpp
template <typename T, bool ThrowOnNull = false, typename... Args>
AtomicSharedPtr<T, ThrowOnNull> make_atomic_shared_ptr(Args&&... args);
```

### Type Traits

```cpp
template <typename T>
struct is_atomic_shared_ptr : std::false_type {};

template <typename T, bool B>
struct is_atomic_shared_ptr<AtomicSharedPtr<T, B>> : std::true_type {};

template <typename T>
inline constexpr bool is_atomic_shared_ptr_v = is_atomic_shared_ptr<T>::value;
```

---

## Summary

AtomicSharedPtr is a **minimal, focused wrapper** for atomic shared_ptr operations.

**Key Characteristics:**

- Single header, zero dependencies
- Works on C++17 and C++20
- Optional null enforcement via template parameter
- Safe default memory orderings
- Native wait/notify on C++20
- 196 lines of auditable code

**Best For:**

- Thread-safe global/shared configuration
- Read-heavy concurrent access patterns
- Codebases spanning C++17 and C++20
- Teams that value simplicity

**Not Ideal For:**

- Atomic weak_ptr (use mutex instead)
- Custom wait semantics (write your own)
- High-contention write scenarios (consider mutex)

**Choose AtomicSharedPtr when:**

- You need atomic shared_ptr access
- You want a clean, portable API
- You value simplicity over features
- 196 lines is enough

---

*AtomicSharedPtr.h (196 lines) — Fat-P Library*
