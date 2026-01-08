---
doc_id: MG-RAII-001
doc_type: "Migration Guide"
title: "Manual Memory Management to RAII"
fatp_components: ["ScopeGuard", "ObjectPool", "AlignedVector"]
topics: ["C migration", "memory management", "malloc", "free", "RAII", "smart pointers", "unique_ptr", "shared_ptr", "ownership semantics"]
constraints: ["manual deallocation discipline", "ownership ambiguity", "double-free", "memory leaks", "exception safety"]
cxx_standard: "C++17"
last_verified: "2025-01-08"
audience: ["C developers", "migration teams", "AI assistants"]
status: "draft"
---

# Migration Guide - Manual Memory Management to RAII

## Scope

This document shows how to migrate C-style manual memory management patterns to automatic RAII using smart pointers and containers. It uses real-world C memory management idioms as case studies, demonstrating that even careful manual memory management cannot prevent leaks and corruption in complex codebases.

## Not Covered

- Custom allocator implementation (see Companion Guide - Allocators)
- Memory pool internals (see User Manual - ObjectPool)
- Aligned memory for SIMD (see User Manual - AlignedVector)
- Reference counting internals (see Companion Guide - shared_ptr Design)
- Garbage collection vs RAII tradeoffs
- Memory-mapped files (see User Manual - MemoryMappedFile)

## Prerequisites

- Familiarity with C memory management (`malloc`, `free`, `realloc`)
- Understanding of C++ object lifetime and scope
- Basic knowledge of C++ templates
- Awareness of exception safety requirements
- Access to standard library headers (`<memory>`, `<vector>`)

---

## Migration Guide Card

**C Pattern:** Manual memory allocation/deallocation with malloc/free, new/delete  
**Why it fails:** Compiler cannot enforce pairing; exceptions bypass deallocation; ownership unclear  
**C++ Solution:** Smart pointers (`unique_ptr`, `shared_ptr`) and containers (`vector`, `string`)  
**Migration effort:** Low to Medium — mechanical replacement of allocation patterns  
**Verification method:** Compile-time ownership tracking; automatic deallocation on scope exit  
**Incremental migration:** Yes — can migrate one allocation at a time; interop with raw pointers  
**Prerequisites:** Understanding of ownership semantics

---

## Table of Contents

1. [The C Patterns](#the-c-patterns)
2. [Why They Fail](#why-they-fail)
3. [The C++ Solutions](#the-c-solutions)
4. [Migration Mechanics](#migration-mechanics)
5. [Verification](#verification)
6. [Performance Characteristics](#performance-characteristics)
7. [Summary](#summary)
8. [Where It Loses](#where-it-loses)
9. [Read Next](#read-next)

---

## The C Patterns

Four related patterns that C programmers use for dynamic memory. All share a common weakness: the compiler cannot track ownership.

### Pattern 1: malloc/free Pairs

**Source:** Every C codebase

```c
/* Allocation and deallocation must be manually paired */
void process_data(size_t size)
{
    char* buffer = malloc(size);
    if (!buffer) {
        return;  /* Allocation failed */
    }
    
    /* Use buffer... */
    process(buffer, size);
    
    free(buffer);  /* Must remember to call this */
}
```

The programmer must ensure every `malloc` has a corresponding `free`, every `calloc` has a `free`, and that `free` is called exactly once.

**Why programmers use it:** Universal, portable, efficient, granular control.

---

### Pattern 2: Array Allocation Patterns

**Source:** Dynamic arrays in C

```c
/* Growing array pattern */
struct DynamicArray {
    int* data;
    size_t size;
    size_t capacity;
};

int array_push(struct DynamicArray* arr, int value)
{
    if (arr->size >= arr->capacity) {
        size_t new_cap = arr->capacity ? arr->capacity * 2 : 8;
        int* new_data = realloc(arr->data, new_cap * sizeof(int));
        if (!new_data) {
            return -1;  /* Failed, original data still valid */
        }
        arr->data = new_data;
        arr->capacity = new_cap;
    }
    arr->data[arr->size++] = value;
    return 0;
}

void array_destroy(struct DynamicArray* arr)
{
    free(arr->data);
    arr->data = NULL;
    arr->size = arr->capacity = 0;
}
```

**Why programmers use it:** Control over growth strategy; no standard alternative in C.

---

### Pattern 3: Ownership Transfer

**Source:** APIs that return allocated memory

```c
/* Caller must free the returned string */
char* format_message(const char* fmt, ...)
{
    char* result = malloc(1024);
    if (!result) return NULL;
    
    va_list args;
    va_start(args, fmt);
    vsnprintf(result, 1024, fmt, args);
    va_end(args);
    
    return result;  /* Ownership transfers to caller */
}

/* Usage: */
char* msg = format_message("Error: %d", errno);
printf("%s\n", msg);
free(msg);  /* Caller responsible for freeing */
```

Documentation says "caller must free" but nothing enforces it.

**Why programmers use it:** Functions need to create and return data.

---

### Pattern 4: Shared Ownership via Reference Counting

**Source:** Any C code with shared resources

```c
struct RefCounted {
    void* data;
    int ref_count;
};

struct RefCounted* rc_create(void* data)
{
    struct RefCounted* rc = malloc(sizeof(*rc));
    if (!rc) return NULL;
    rc->data = data;
    rc->ref_count = 1;
    return rc;
}

void rc_acquire(struct RefCounted* rc)
{
    rc->ref_count++;  /* Not thread-safe! */
}

void rc_release(struct RefCounted* rc)
{
    if (--rc->ref_count == 0) {  /* Not thread-safe! */
        free(rc->data);
        free(rc);
    }
}
```

**Why programmers use it:** Multiple owners need access to same resource.

**Problem:** Manual reference counting is error-prone, typically not thread-safe.

---

### The Common Thread

All patterns share a fatal weakness: **nothing enforces correct usage**.

| Pattern | Enforcement | Weakness |
|---------|-------------|----------|
| malloc/free | None | Double-free, use-after-free, leaks |
| Array realloc | None | Pointer invalidation, leaks on failure |
| Ownership transfer | Documentation | Nothing enforces who frees |
| Reference counting | Manual discipline | Race conditions, count errors |

---

## Why They Fail

### The Double-Free

Using memory after freeing, or freeing twice:

```c
void process(void)
{
    char* buf = malloc(100);
    
    if (error_condition) {
        free(buf);
        return;
    }
    
    /* ... more code ... */
    
    free(buf);  /* Bug if error_condition was true: double-free */
}
```

### The Memory Leak

Forgetting to free, especially on error paths:

```c
int process(const char* path)
{
    FILE* f = fopen(path, "r");
    char* buf = malloc(1024);
    
    if (!f) {
        /* Bug: buf leaked! */
        return -1;
    }
    
    if (!buf) {
        fclose(f);
        return -1;
    }
    
    if (read_data(f, buf) < 0) {
        /* Bug: both f and buf leaked! */
        return -1;
    }
    
    fclose(f);
    free(buf);
    return 0;
}
```

### The Exception Leak

In C++, any function might throw:

```cpp
void process()
{
    char* buf = static_cast<char*>(malloc(1024));
    
    std::vector<int> v;
    v.reserve(1000000);  // Might throw std::bad_alloc!
    
    // If reserve() throws, buf is leaked
    
    free(buf);
}
```

### The Dangling Pointer

Using memory after it's freed:

```c
struct Node {
    int value;
    struct Node* next;
};

void process_list(struct Node* head)
{
    struct Node* current = head;
    while (current) {
        struct Node* next = current->next;
        if (should_remove(current)) {
            free(current);
        }
        current = next;  /* Bug if current was freed: use-after-free */
    }
}
```

---

## The C++ Solutions

### Solution 1: std::unique_ptr (Exclusive Ownership)

Single owner; automatically freed when owner goes out of scope:

```cpp
#include <memory>

void process_data(size_t size)
{
    auto buffer = std::make_unique<char[]>(size);
    // No null check needed - throws on failure
    
    process(buffer.get(), size);
    
    // Automatically freed here - no explicit free() needed
}
```

### Solution 2: std::shared_ptr (Shared Ownership)

Multiple owners; freed when last owner releases:

```cpp
#include <memory>

std::shared_ptr<Resource> create_shared_resource()
{
    return std::make_shared<Resource>(args...);
}

void use_resource()
{
    auto r1 = create_shared_resource();  // ref_count = 1
    {
        auto r2 = r1;  // ref_count = 2
        use(r2);
    }  // ref_count = 1 (r2 destroyed)
    
    use(r1);
}  // ref_count = 0, Resource destroyed
```

### Solution 3: std::vector (Dynamic Arrays)

Automatic growth, automatic cleanup:

```cpp
#include <vector>

void process_items()
{
    std::vector<int> items;
    
    items.push_back(1);  // Automatic growth
    items.push_back(2);
    items.reserve(100);  // Pre-allocate if needed
    
    for (int i : items) {
        process(i);
    }
    
    // Automatically freed - no explicit cleanup needed
}
```

### Solution 4: std::string (Dynamic Strings)

Automatic memory management for text:

```cpp
#include <string>

std::string format_message(const char* fmt, ...)
{
    // Return by value - no ownership confusion
    return std::string(fmt) + " processed";
}

void use_message()
{
    std::string msg = format_message("Error");
    std::cout << msg << "\n";
    // Automatically freed - no free() needed
}
```

---

## Migration Mechanics

### Step-by-Step: malloc/free to unique_ptr

**Step 1: Identify the pattern**

```c
void process(void)
{
    Widget* w = malloc(sizeof(Widget));
    if (!w) return;
    
    widget_init(w);
    widget_use(w);
    widget_cleanup(w);
    
    free(w);
}
```

**Step 2: Choose the right smart pointer**

| Ownership | Smart Pointer |
|-----------|--------------|
| Single owner, transferred via move | `std::unique_ptr` |
| Multiple owners, shared lifetime | `std::shared_ptr` |
| Non-owning reference to shared | `std::weak_ptr` |
| Non-owning, guaranteed valid | Raw pointer or reference |

**Step 3: Replace allocation**

```cpp
// For C structs with no constructor:
auto w = std::unique_ptr<Widget>(static_cast<Widget*>(malloc(sizeof(Widget))));

// Better: use make_unique with C++ class:
auto w = std::make_unique<Widget>(args...);
```

**Step 4: Handle custom cleanup**

```cpp
// If cleanup isn't just free(), use custom deleter:
auto w = std::unique_ptr<Widget, decltype(&widget_destroy)>(
    widget_create(),
    widget_destroy
);

// Or with lambda:
auto w = std::unique_ptr<Widget, void(*)(Widget*)>(
    widget_create(),
    [](Widget* p) {
        widget_cleanup(p);
        free(p);
    }
);
```

**Step 5: Update call sites**

```cpp
// Before
Widget* w = create_widget();
use_widget(w);
destroy_widget(w);

// After
auto w = create_widget_unique();  // Returns unique_ptr<Widget>
use_widget(w.get());              // Pass raw pointer to non-owning functions
// Automatically destroyed
```

### Step-by-Step: Arrays to vector

**Step 1: Identify the pattern**

```c
struct DynamicArray {
    int* data;
    size_t size;
    size_t capacity;
};
```

**Step 2: Replace with std::vector**

```cpp
// Just use vector directly:
std::vector<int> data;

// If you need the C array interface:
int* raw_data = data.data();
size_t size = data.size();
size_t capacity = data.capacity();
```

**Step 3: Migration of operations**

| C Operation | C++ Equivalent |
|------------|----------------|
| `malloc(n * sizeof(T))` | `std::vector<T> v; v.reserve(n);` |
| `realloc(p, new_size)` | `v.resize(new_size);` or `v.reserve(new_size);` |
| `arr->data[i]` | `v[i]` or `v.at(i)` |
| `arr->size++; arr->data[arr->size-1] = x;` | `v.push_back(x);` |
| `free(arr->data)` | Automatic |

**Step 4: Preserve C compatibility at boundaries**

```cpp
// When calling C functions that need raw array:
std::vector<int> data = {1, 2, 3, 4};
c_function(data.data(), data.size());  // Pass raw pointer
```

### Step-by-Step: Ownership Transfer

**Step 1: Identify the transfer pattern**

```c
/* Returns allocated memory; caller must free */
Widget* create_widget(void);
```

**Step 2: Return unique_ptr to express ownership**

```cpp
// Ownership is explicit in the type:
std::unique_ptr<Widget> create_widget();

// Usage:
auto w = create_widget();  // Caller owns
use_widget(w.get());       // Non-owning access
// Automatically freed when w goes out of scope
```

**Step 3: Transfer ownership explicitly**

```cpp
// Transfer to another owner:
auto w1 = create_widget();
auto w2 = std::move(w1);  // w1 is now null, w2 owns

// Transfer to a function:
void take_ownership(std::unique_ptr<Widget> w);
take_ownership(std::move(w1));  // w1 is now null

// Release back to raw pointer (for C interop):
Widget* raw = w1.release();  // w1 is now null, caller must manage raw
```

### Step-by-Step: Reference Counting

**Step 1: Identify manual reference counting**

```c
struct RefCounted {
    int ref_count;
    Data* data;
};
```

**Step 2: Replace with shared_ptr**

```cpp
// shared_ptr handles reference counting automatically:
auto data = std::make_shared<Data>(args...);
auto copy = data;  // ref_count = 2 (thread-safe!)
```

**Step 3: Handle weak references**

```cpp
// For non-owning references that don't prevent destruction:
std::weak_ptr<Data> observer = data;

// Later, check if still alive:
if (auto locked = observer.lock()) {
    // Data still exists, use locked
    locked->method();
}
// else: Data was destroyed
```

### Interop with C APIs

**Pattern 1: Wrapping C allocation**

```cpp
// C library allocates, we wrap for RAII:
auto* raw = c_library_create();
auto wrapped = std::unique_ptr<CObject, decltype(&c_library_destroy)>(
    raw, c_library_destroy
);
```

**Pattern 2: Giving to C API (transfer out)**

```cpp
std::unique_ptr<Widget> w = create_widget();
// Transfer ownership to C library:
c_library_take_ownership(w.release());  // Now null, C owns it
```

**Pattern 3: Borrowing to C API (non-owning)**

```cpp
std::unique_ptr<Widget> w = create_widget();
// Let C library use it (doesn't take ownership):
c_library_use(w.get());  // We still own it
```

### Using Fat-P Components

For specialized allocation patterns, Fat-P provides additional components:

```cpp
#include "ObjectPool.h"
#include "AlignedVector.h"

// Object pool for frequent alloc/dealloc of same type:
fat_p::ObjectPool<Widget> pool(100);  // Pre-allocate 100
auto* w = pool.acquire();
pool.release(w);

// Aligned vector for SIMD operations:
fat_p::AlignedVector<float, 32> simd_data(1024);  // 32-byte aligned
```

---

## Verification

### Compile-Time Guarantees

Smart pointers provide compile-time ownership tracking:

```cpp
std::unique_ptr<Widget> w = create_widget();

// ERROR: Cannot copy unique_ptr
std::unique_ptr<Widget> w2 = w;

// OK: Must explicitly transfer
std::unique_ptr<Widget> w2 = std::move(w);
// w is now null - compiler doesn't prevent use, but sanitizers catch it

// ERROR: Cannot pass unique_ptr by value without move
take_ownership(w);  // Error if take_ownership takes by value

// OK: Explicit transfer
take_ownership(std::move(w));
```

### Runtime Validation

| Scenario | unique_ptr Behavior | shared_ptr Behavior |
|----------|--------------------|--------------------|
| Out of scope | Deleter called | Decrement ref_count, delete if 0 |
| Move from | Source becomes null | Source becomes null |
| Double delete | Impossible (single owner) | Impossible (ref counted) |
| Use after move | Null dereference (sanitizer catches) | Null dereference |

### Recommended Tests

```cpp
#include "FatPTest.h"
#include <memory>

namespace fat_p::testing::raii
{

TEST_CASE(unique_ptr_auto_cleanup)
{
    static bool destroyed = false;
    struct TrackedWidget {
        ~TrackedWidget() { destroyed = true; }
    };
    
    destroyed = false;
    {
        auto w = std::make_unique<TrackedWidget>();
    }
    ASSERT_TRUE(destroyed, "unique_ptr should destroy on scope exit");
    return true;
}

TEST_CASE(shared_ptr_ref_counting)
{
    static int destroy_count = 0;
    struct TrackedWidget {
        ~TrackedWidget() { destroy_count++; }
    };
    
    destroy_count = 0;
    {
        auto w1 = std::make_shared<TrackedWidget>();
        ASSERT_EQ(w1.use_count(), 1L, "Single owner");
        {
            auto w2 = w1;
            ASSERT_EQ(w1.use_count(), 2L, "Two owners");
        }
        ASSERT_EQ(w1.use_count(), 1L, "Back to single owner");
        ASSERT_EQ(destroy_count, 0, "Not destroyed yet");
    }
    ASSERT_EQ(destroy_count, 1, "Destroyed when last owner gone");
    return true;
}

TEST_CASE(unique_ptr_move_semantics)
{
    auto w1 = std::make_unique<int>(42);
    ASSERT_TRUE(w1 != nullptr, "Should be valid");
    
    auto w2 = std::move(w1);
    ASSERT_TRUE(w1 == nullptr, "Moved-from should be null");
    ASSERT_TRUE(w2 != nullptr, "Moved-to should be valid");
    ASSERT_EQ(*w2, 42, "Value should be preserved");
    return true;
}

TEST_CASE(vector_automatic_growth_and_cleanup)
{
    std::vector<int> v;
    
    for (int i = 0; i < 1000; ++i) {
        v.push_back(i);  // Automatic growth
    }
    
    ASSERT_EQ(v.size(), 1000u, "Should have 1000 elements");
    ASSERT_GE(v.capacity(), 1000u, "Capacity should accommodate");
    
    // Cleanup is automatic - no leak even if we return early
    return true;
}

TEST_CASE(custom_deleter_c_interop)
{
    static bool custom_delete_called = false;
    
    auto deleter = [](int* p) {
        custom_delete_called = true;
        delete p;
    };
    
    custom_delete_called = false;
    {
        std::unique_ptr<int, decltype(deleter)> p(new int(42), deleter);
    }
    ASSERT_TRUE(custom_delete_called, "Custom deleter should be called");
    return true;
}

} // namespace
```

### AddressSanitizer Catches

With ASan enabled, RAII violations are caught at runtime:

```cpp
auto w = std::make_unique<Widget>();
Widget* raw = w.get();
w.reset();  // Deletes the widget
raw->method();  // ASan: heap-use-after-free
```

---

## Performance Characteristics

### Overhead Measurements

| Pattern | Allocation | Deallocation | Access |
|---------|------------|--------------|--------|
| Raw malloc/free | ~50 ns | ~30 ns | 0 ns |
| unique_ptr | ~50 ns | ~30 ns | 0 ns (identical) |
| shared_ptr | ~80 ns | ~40 ns | ~0.5 ns (atomic load) |
| make_shared | ~60 ns | ~40 ns | ~0.5 ns (single alloc) |
| vector push_back | ~2 ns amortized | Auto | ~0.5 ns |

**Source:** Typical measurements on modern x64 hardware.

### Memory Overhead

| Type | Overhead |
|------|----------|
| Raw pointer | 0 |
| unique_ptr (default) | 0 (same size as raw pointer) |
| unique_ptr (custom deleter) | sizeof(deleter) |
| shared_ptr | 16-24 bytes control block |
| make_shared | 8-16 bytes control block (merged) |
| vector | 24 bytes (ptr + size + capacity) |

### When Raw Pointers Are Still Appropriate

| Scenario | Recommendation |
|----------|----------------|
| Non-owning parameter | Raw pointer or reference |
| Hot loop with known lifetime | Raw pointer |
| C interop at boundaries | Raw pointer |
| Stack allocation | Value or reference |
| Static lifetime | Raw pointer or static |

---

## Summary

| Aspect | C Pattern | C++ with RAII |
|--------|-----------|---------------|
| Ownership | Documentation only | Encoded in type system |
| Deallocation | Manual, error-prone | Automatic, guaranteed |
| Exception safety | None | Full |
| Double-free | Possible | Impossible |
| Memory leaks | Common | Compile-time prevention |
| Thread safety (shared) | Manual | Atomic ref counting |
| Runtime overhead | 0 | 0 (unique_ptr), minimal (shared_ptr) |

---

## Where It Loses

- **C interop overhead:** Wrapping C allocations adds code; sometimes raw pointers at boundaries are cleaner.

- **shared_ptr atomic overhead:** Reference count operations are atomic. In single-threaded code, this is wasted. Consider `boost::local_shared_ptr` or similar.

- **Custom allocators:** Standard smart pointers work with custom allocators, but the syntax is verbose.

- **Non-memory resources:** Smart pointers are for memory. For files, sockets, etc., use `ScopeGuard` or dedicated RAII wrappers.

- **Intrusive ref counting:** `shared_ptr` uses external control block. Some designs need intrusive counting (e.g., COM). Use `boost::intrusive_ptr` or custom.

- **Circular references:** `shared_ptr` cycles leak. Break with `weak_ptr` or redesign ownership.

- **Performance-critical code:** In extreme hot paths, smart pointer overhead may matter. Profile first.

---

## Read Next

- **User Manual - ObjectPool** — High-performance pooled allocation
- **User Manual - AlignedVector** — SIMD-friendly aligned memory
- **Migration Guide - Manual Cleanup to ScopeGuard** — RAII for non-memory resources
- **Companion Guide - Smart Pointer Design** — When to use which pointer
- **Handbook - Ownership Semantics** — Designing ownership hierarchies

---

*Migration Guide - Manual Memory Management to RAII v1.0 — January 2025*
