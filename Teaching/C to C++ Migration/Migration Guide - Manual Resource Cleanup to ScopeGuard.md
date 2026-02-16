---
doc_id: MG-SCOPEGUARD-001
doc_type: "Migration Guide"
title: "Manual Resource Cleanup to ScopeGuard"
fatp_components: ["ScopeGuard"]
topics: ["C migration", "resource management", "goto cleanup", "RAII", "exception safety", "scope exit"]
constraints: ["manual cleanup ordering", "exception path coverage", "early return leaks", "cleanup duplication"]
cxx_standard: "C++20"
last_verified: "2025-01-08"
audience: ["C developers", "migration teams", "AI assistants"]
status: "draft"
---

# Migration Guide - Manual Resource Cleanup to ScopeGuard

## Scope

This document shows how to migrate C-style manual cleanup patterns to automatic RAII cleanup using `ScopeGuard`. It uses the Linux kernel's "goto cleanup" idiom and common C resource management as case studies, demonstrating that even disciplined manual cleanup cannot prevent resource leaks in the presence of exceptions or complex control flow.

## Not Covered

- Full `ScopeGuard` API reference (see User Manual - ScopeGuard)
- Policy implementation details (see Companion Guide - ScopeGuard)
- Smart pointers for heap allocation (`std::unique_ptr`, `std::shared_ptr`)
- Other Fat-P RAII components (`Expected<T>`, `EnforcedInit<T>`)
- Custom deleter patterns for third-party resources
- Async resource cleanup

## Prerequisites

- Familiarity with C resource management patterns (goto cleanup, nested if-else)
- Understanding of C++ scope and object lifetime
- Awareness of exception safety requirements
- Access to Fat-P headers

---

## Migration Guide Card

**From:** Manual resource cleanup via `goto` labels, nested conditionals, or explicit release calls  
**To:** `ScopeGuard` — RAII wrapper that executes cleanup on scope exit, regardless of exit path  
**Why migrate:** Exceptions bypass manual cleanup; early returns require discipline; cleanup ordering is error-prone and invisible  
**Compatibility strategy:** Incremental — wrap cleanup code in lambda; no structural changes to surrounding code  
**Mechanical steps:**
1. Identify resource acquisition followed by manual cleanup (`goto cleanup`, nested `if`, explicit `close()`).
2. Replace cleanup logic with `ScopeGuard` capturing the release operation.
3. Use `dismiss()` for conditional cleanup (commit/rollback patterns).
4. Remove `goto` labels and nested cleanup conditionals.
**Behavioral equivalence:** Same resources acquired and released in same order  
**Intentional differences:** Cleanup executes on all exit paths (exceptions, early returns, normal flow); no missed cleanup  
**Failure model:** Cleanup failure in guard destructor — policy-selectable (log, terminate, swallow)  
**Threading model:** Unchanged — `ScopeGuard` is scope-local with no synchronization requirements  
**Lifetime model:** Guard lives on the stack; cleanup executes when guard goes out of scope  
**Alternatives:** `std::experimental::scope_exit` (Library Fundamentals TS), Boost.ScopeExit, manual RAII wrapper classes  
**Verification:** Unit tests for normal exit, exception exit, early return, and `dismiss()` paths; sanitizer runs  
**Rollback plan:** Replace `ScopeGuard` with manual cleanup; restore `goto` labels or nested conditionals

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

Three related patterns that C programmers use to manage resource cleanup. All share a common weakness: they assume all exit paths are visible at the cleanup site.

### Pattern 1: The goto Cleanup Idiom

**Source:** Linux kernel coding style, countless C codebases

Resources are acquired in sequence, and a chain of goto labels handles cleanup:

```c
/* Linux kernel style - from Documentation/process/coding-style.rst */
int process_data(const char* path)
{
    int ret = -ENOMEM;
    char* buf = NULL;
    FILE* file = NULL;
    struct parser* parser = NULL;
    
    buf = malloc(BUFFER_SIZE);
    if (!buf)
        goto out;
    
    file = fopen(path, "r");
    if (!file)
        goto out_free_buf;
    
    parser = parser_create();
    if (!parser)
        goto out_close_file;
    
    /* Actual work */
    ret = do_parsing(parser, file, buf);
    
    parser_destroy(parser);
out_close_file:
    fclose(file);
out_free_buf:
    free(buf);
out:
    return ret;
}
```

Each acquisition point has a corresponding label. Failure at any point jumps to the appropriate cleanup chain.

**Why programmers use it:** Linear code flow; cleanup order is explicit; avoids deep nesting.

---

### Pattern 2: Nested Conditional Cleanup

**Source:** Traditional C error handling

Resources are acquired in nested if-else blocks:

```c
int process_data(const char* path)
{
    int ret = -1;
    char* buf = malloc(BUFFER_SIZE);
    
    if (buf) {
        FILE* file = fopen(path, "r");
        
        if (file) {
            struct parser* parser = parser_create();
            
            if (parser) {
                ret = do_parsing(parser, file, buf);
                parser_destroy(parser);
            }
            fclose(file);
        }
        free(buf);
    }
    return ret;
}
```

**Why programmers use it:** No gotos; cleanup naturally pairs with acquisition.

**Problem:** Deep nesting; hard to read; easy to miss cleanup on error paths.

---

### Pattern 3: Manual Release with Early Return

**Source:** Common in quick-and-dirty C code

```c
/* Dangerous pattern - easy to leak */
int process_data(const char* path)
{
    char* buf = malloc(BUFFER_SIZE);
    FILE* file = fopen(path, "r");
    
    if (!buf || !file) {
        free(buf);      /* May be NULL - that's OK */
        if (file) fclose(file);
        return -1;
    }
    
    if (!validate_file(file)) {
        /* Bug: forgot to free buf and close file! */
        return -EINVAL;
    }
    
    /* More processing... */
    
    fclose(file);
    free(buf);
    return 0;
}
```

**Why programmers use it:** Quick to write; feels natural.

**Problem:** Every early return must remember every resource. Maintenance nightmare.

---

### The Common Thread

All three patterns share a fatal weakness: **exceptions bypass all cleanup mechanisms**.

| Pattern | Enforcement Mechanism | C++ Exception Weakness |
|---------|----------------------|------------------------|
| goto cleanup | Jump to labels | Exceptions skip gotos entirely |
| Nested if-else | Scope-based cleanup | Exceptions unwind without else blocks |
| Early return cleanup | Manual at each return | Exceptions aren't returns |

In C, exceptions don't exist, so these patterns work. In C++, any function call might throw, making every manual cleanup pattern potentially leaky.

---

## Why They Fail

### The Invisible Exception Path

C++ code can throw from almost anywhere:

```cpp
int process_data(const std::string& path)  // string copy might throw
{
    char* buf = malloc(BUFFER_SIZE);
    FILE* file = fopen(path.c_str(), "r");
    
    if (!buf || !file) {
        free(buf);
        if (file) fclose(file);
        return -1;
    }
    
    std::vector<int> results;
    results.reserve(100);  // Might throw std::bad_alloc!
    
    // If reserve() throws, buf and file are leaked
    
    fclose(file);
    free(buf);
    return 0;
}
```

Even in "C-style" C++ code, exceptions can be thrown by standard library types, operator new, and any function not marked `noexcept`.

### The Forgotten Cleanup Path

Manual cleanup requires updating every exit path when adding resources:

```c
int process_data(const char* path)
{
    char* buf = malloc(BUFFER_SIZE);
    FILE* file = fopen(path, "r");
    
    // Later: add a new resource
    int socket = connect_server();
    
    if (!buf || !file || socket < 0) {
        free(buf);
        if (file) fclose(file);
        // Bug: forgot to close socket!
        return -1;
    }
    
    // Every existing error path must be updated
    if (!validate_file(file)) {
        free(buf);
        fclose(file);
        close(socket);  // Must remember to add this everywhere
        return -EINVAL;
    }
    
    // ... more paths to update ...
}
```

### The Order Dependency Bug

Cleanup must happen in reverse acquisition order. Getting this wrong causes use-after-free:

```c
/* Bug: cleanup order reversed */
int process_data(void)
{
    struct A* a = create_a();
    struct B* b = create_b(a);  /* b depends on a */
    
    /* ... work ... */
    
    destroy_a(a);  /* Bug: b still references a */
    destroy_b(b);  /* Use-after-free in destroy_b */
    return 0;
}
```

---

## The C++ Solution

`ScopeGuard` executes cleanup code when the scope exits, regardless of how it exits: normal return, early return, exception, or any other control flow.

### Basic Usage

```cpp
#include "ScopeGuard.h"

void process_data(const std::string& path)
{
    char* buf = static_cast<char*>(malloc(BUFFER_SIZE));
    SCOPE_EXIT { free(buf); };  // Always runs
    
    FILE* file = fopen(path.c_str(), "r");
    if (!file) throw std::runtime_error("Cannot open file");
    SCOPE_EXIT { fclose(file); };  // Always runs
    
    auto parser = parser_create();
    if (!parser) throw std::runtime_error("Cannot create parser");
    SCOPE_EXIT { parser_destroy(parser); };  // Always runs
    
    // If any of these throw, all resources are cleaned up
    std::vector<int> results;
    results.reserve(100);
    do_parsing(parser, file, buf, results);
    
    // Normal exit: all SCOPE_EXIT blocks run in reverse order
}
```

### Key Properties

| Property | Behavior |
|----------|----------|
| Execution | On any scope exit (return, throw, fall-through) |
| Order | Reverse declaration order (LIFO) |
| `dismiss()` | Prevents execution (for transfer semantics) |
| `SCOPE_FAIL` | Executes only when unwinding due to exception |
| `SCOPE_SUCCESS` | Executes only on normal exit (no exception) |

### The Three Guard Types

```cpp
void transaction_example()
{
    begin_transaction();
    
    // SCOPE_EXIT: Always runs (cleanup)
    SCOPE_EXIT { close_connection(); };
    
    // SCOPE_FAIL: Only on exception (rollback)
    SCOPE_FAIL { rollback_transaction(); };
    
    // SCOPE_SUCCESS: Only on normal exit (commit)
    SCOPE_SUCCESS { commit_transaction(); };
    
    do_work();  // May throw
    
    // If do_work() succeeds: commit runs, then close
    // If do_work() throws: rollback runs, then close
}
```

---

## Migration Mechanics

### Step-by-Step: goto Cleanup

**Step 1: Identify the pattern**

```c
int process(void)
{
    int ret = -1;
    Resource* a = NULL;
    Resource* b = NULL;
    
    a = acquire_a();
    if (!a) goto out;
    
    b = acquire_b();
    if (!b) goto out_release_a;
    
    ret = do_work(a, b);
    
    release_b(b);
out_release_a:
    release_a(a);
out:
    return ret;
}
```

**Step 2: Replace with SCOPE_EXIT**

```cpp
int process()
{
    Resource* a = acquire_a();
    if (!a) return -1;
    SCOPE_EXIT { release_a(a); };
    
    Resource* b = acquire_b();
    if (!b) return -1;  // a is cleaned up automatically
    SCOPE_EXIT { release_b(b); };
    
    return do_work(a, b);
    // Both guards run in reverse order: release_b, then release_a
}
```

**Step 3: Note the improvements**
- No labels or gotos
- Early return is safe
- Exceptions are safe
- Cleanup order is automatic (reverse declaration)

### Step-by-Step: Conditional Cleanup

**Step 1: Identify the pattern**

```c
/* Cleanup only on failure */
int init_subsystem(void)
{
    if (init_a() < 0)
        return -1;
    
    if (init_b() < 0) {
        cleanup_a();  /* Rollback a */
        return -1;
    }
    
    if (init_c() < 0) {
        cleanup_b();  /* Rollback b */
        cleanup_a();  /* Rollback a */
        return -1;
    }
    
    return 0;  /* Success - don't cleanup */
}
```

**Step 2: Use dismiss() or SCOPE_FAIL**

```cpp
// Option A: Using dismiss()
int init_subsystem()
{
    if (init_a() < 0) return -1;
    auto guard_a = fat_p::makeScopeGuard([]{ cleanup_a(); });
    
    if (init_b() < 0) return -1;  // guard_a runs
    auto guard_b = fat_p::makeScopeGuard([]{ cleanup_b(); });
    
    if (init_c() < 0) return -1;  // guard_b, then guard_a run
    
    // Success - dismiss all guards
    guard_a.dismiss();
    guard_b.dismiss();
    return 0;
}

// Option B: Using SCOPE_FAIL (cleaner)
int init_subsystem()
{
    if (init_a() < 0) return -1;
    SCOPE_FAIL { cleanup_a(); };
    
    if (init_b() < 0) return -1;
    SCOPE_FAIL { cleanup_b(); };
    
    if (init_c() < 0) return -1;
    
    return 0;  // SCOPE_FAIL blocks don't run on normal return
}
```

### Step-by-Step: Transaction Pattern

**Step 1: Identify the pattern**

```c
int transfer_money(Account* from, Account* to, int amount)
{
    if (withdraw(from, amount) < 0)
        return -1;
    
    if (deposit(to, amount) < 0) {
        /* Rollback: must restore 'from' account */
        deposit(from, amount);
        return -1;
    }
    
    return 0;
}
```

**Step 2: Use SCOPE_FAIL for rollback**

```cpp
int transfer_money(Account& from, Account& to, int amount)
{
    withdraw(from, amount);  // May throw
    SCOPE_FAIL { deposit(from, amount); };  // Rollback on failure
    
    deposit(to, amount);  // May throw - triggers rollback if so
    
    return 0;  // Success - no rollback
}
```

### Using makeScopeGuard with Policies

For fine-grained control, use factory functions with policies:

```cpp
#include "ScopeGuard.h"
#include "ScopeGuardPolicies.h"

void example()
{
    FILE* f = fopen("data.txt", "r");
    
    // Default policy: std::terminate if cleanup throws
    auto guard1 = fat_p::makeScopeGuard([f]{ fclose(f); });
    
    // Log policy: log and swallow exceptions from cleanup
    auto guard2 = fat_p::makeScopeGuard<fat_p::ScopeGuardLogAndSwallowPolicy>(
        [f]{ fclose(f); }
    );
    
    // Nothrow policy: cleanup must be noexcept
    auto guard3 = fat_p::makeScopeGuard<fat_p::ScopeGuardNothrowPolicy>(
        [f]{ fclose(f); }
    );
}
```

### Transfer Semantics with dismiss()

When ownership transfers, dismiss the guard:

```cpp
class ResourceOwner {
    Resource* m_resource = nullptr;
    
public:
    void acquire()
    {
        Resource* r = create_resource();
        SCOPE_FAIL { destroy_resource(r); };
        
        validate(r);  // May throw
        
        m_resource = r;
        // Don't call dismiss() - SCOPE_FAIL only runs on exception
    }
    
    Resource* release()
    {
        Resource* r = m_resource;
        m_resource = nullptr;
        return r;  // Caller now owns it
    }
    
    ~ResourceOwner()
    {
        if (m_resource) {
            destroy_resource(m_resource);
        }
    }
};
```

### Common Mistakes

| Mistake | Symptom | Fix |
|---------|---------|-----|
| Using `return` inside SCOPE_EXIT | Returns from lambda, not function | Don't use control flow in cleanup |
| Capturing by reference after scope ends | Dangling reference | Capture by value or ensure lifetime |
| Forgetting dismiss() for conditional cleanup | Double cleanup | Use SCOPE_FAIL instead, or call dismiss() |
| Throwing from cleanup during unwinding | std::terminate | Use try-catch in cleanup or nothrow policy |
| Cleanup order dependency not matching declaration | Resource use-after-free | Declare guards in acquisition order |

### Lambda Capture Warning

**CRITICAL:** `return` inside a SCOPE_* block returns from the lambda, not the enclosing function:

```cpp
void dangerous()
{
    SCOPE_EXIT {
        if (error_condition) {
            return;  // WRONG: Returns from lambda, cleanup continues!
        }
        do_cleanup();
    };
    // ...
}

void correct()
{
    SCOPE_EXIT {
        if (!error_condition) {
            do_cleanup();
        }
        // No return - just conditional execution
    };
    // ...
}
```

---

## Verification

### Compile-Time Guarantees

`ScopeGuard` provides structural guarantees:

```cpp
// Guard is [[nodiscard]] - must be assigned to a variable
auto guard = fat_p::makeScopeGuard([]{ cleanup(); });  // OK
fat_p::makeScopeGuard([]{ cleanup(); });  // Warning: discarded

// Cleanup runs even if you forget dismiss()
void example() {
    auto guard = fat_p::makeScopeGuard([]{ cleanup(); });
    // forgot guard.dismiss()
}  // cleanup() runs anyway - usually desired behavior
```

### Runtime Validation

| Scenario | Behavior |
|----------|----------|
| Normal scope exit | Cleanup runs |
| Early return | Cleanup runs |
| Exception thrown | Cleanup runs |
| `dismiss()` called | Cleanup does NOT run |
| `SCOPE_FAIL` without exception | Cleanup does NOT run |
| `SCOPE_SUCCESS` with exception | Cleanup does NOT run |

### Recommended Tests

```cpp
#include "FatPTest.h"
#include "ScopeGuard.h"

namespace fat_p::testing::scopeguard
{

TEST_CASE(cleanup_runs_on_normal_exit)
{
    bool cleaned = false;
    {
        SCOPE_EXIT { cleaned = true; };
    }
    ASSERT_TRUE(cleaned, "Cleanup should run on normal scope exit");
    return true;
}

TEST_CASE(cleanup_runs_on_exception)
{
    bool cleaned = false;
    
    try {
        SCOPE_EXIT { cleaned = true; };
        throw std::runtime_error("test");
    } catch (...) {
        // Expected
    }
    
    ASSERT_TRUE(cleaned, "Cleanup should run when exception thrown");
    return true;
}

TEST_CASE(dismiss_prevents_cleanup)
{
    bool cleaned = false;
    {
        auto guard = fat_p::makeScopeGuard([&]{ cleaned = true; });
        guard.dismiss();
    }
    ASSERT_FALSE(cleaned, "Cleanup should NOT run after dismiss()");
    return true;
}

TEST_CASE(scope_fail_only_on_exception)
{
    bool failed = false;
    
    // Normal exit - should NOT run
    {
        SCOPE_FAIL { failed = true; };
    }
    ASSERT_FALSE(failed, "SCOPE_FAIL should not run on normal exit");
    
    // Exception exit - should run
    try {
        SCOPE_FAIL { failed = true; };
        throw std::runtime_error("test");
    } catch (...) {}
    
    ASSERT_TRUE(failed, "SCOPE_FAIL should run on exception");
    return true;
}

TEST_CASE(scope_success_only_on_normal_exit)
{
    bool succeeded = false;
    
    // Normal exit - should run
    {
        SCOPE_SUCCESS { succeeded = true; };
    }
    ASSERT_TRUE(succeeded, "SCOPE_SUCCESS should run on normal exit");
    
    // Exception exit - should NOT run
    succeeded = false;
    try {
        SCOPE_SUCCESS { succeeded = true; };
        throw std::runtime_error("test");
    } catch (...) {}
    
    ASSERT_FALSE(succeeded, "SCOPE_SUCCESS should not run on exception");
    return true;
}

TEST_CASE(cleanup_order_is_lifo)
{
    std::vector<int> order;
    {
        SCOPE_EXIT { order.push_back(1); };
        SCOPE_EXIT { order.push_back(2); };
        SCOPE_EXIT { order.push_back(3); };
    }
    
    ASSERT_EQ(order.size(), 3u, "All cleanups should run");
    ASSERT_EQ(order[0], 3, "Last declared runs first");
    ASSERT_EQ(order[1], 2, "Middle runs second");
    ASSERT_EQ(order[2], 1, "First declared runs last");
    return true;
}

} // namespace
```

---

## Performance Characteristics

### Overhead Measurements

| Operation | Cost | Notes |
|-----------|------|-------|
| Guard construction | ~1-2 ns | Lambda capture + bool init |
| Guard destruction (runs) | ~1 ns + cleanup | Inline cleanup call |
| Guard destruction (dismissed) | ~0.3 ns | Bool check only |
| `dismiss()` call | ~0 ns | Single bool write |

**Source:** Fat-P benchmark suite internal measurements.

### Memory Layout

```cpp
// ScopeGuard with lambda: sizeof(lambda) + 1 byte (bool) + padding
// Typical lambda with pointer capture: 8 bytes + 1 + 7 padding = 16 bytes

// ScopeGuardOnFail/OnSuccess: sizeof(lambda) + int (uncaught count) + bool + padding
```

### Optimization Notes

Modern compilers optimize simple ScopeGuard usage to zero overhead:

```cpp
void example(FILE* f)
{
    SCOPE_EXIT { fclose(f); };
    process(f);
}

// Optimizes to approximately:
void example_optimized(FILE* f)
{
    try {
        process(f);
        fclose(f);
    } catch (...) {
        fclose(f);
        throw;
    }
}

// Which further optimizes to duplicate cleanup at each exit point
```

### Comparison to Manual Pattern

| Approach | Setup Cost | Cleanup Cost | Exception Safety |
|----------|------------|--------------|------------------|
| goto cleanup | 0 | 0 + cleanup | None |
| Nested if-else | 0 | 0 + cleanup | None |
| ScopeGuard | ~1 ns | ~1 ns + cleanup | Full |
| SCOPE_FAIL | ~2 ns | ~1 ns + cleanup | Full |

The overhead is negligible compared to typical cleanup operations (fclose, free, etc.).

---

## Summary

| Aspect | C Pattern | C++ with ScopeGuard |
|--------|-----------|---------------------|
| Cleanup trigger | Manual at each exit point | Automatic on scope exit |
| Exception safety | None | Full |
| Early return safety | Must remember cleanup | Automatic |
| Cleanup order | Manual (error-prone) | Automatic (LIFO) |
| Conditional cleanup | Complex if-else chains | `dismiss()` or SCOPE_FAIL |
| Runtime cost | 0 | ~1-2 ns per guard |
| Code locality | Cleanup far from acquisition | Cleanup next to acquisition |

---

## Where It Loses

- **Cleanup code visibility:** Cleanup is at acquisition site, not at end of function. Some prefer seeing all cleanup together (goto style).

- **Lambda capture overhead:** Each guard captures its cleanup context. For many resources, this adds stack usage.

- **Cannot return from cleanup:** `return` inside SCOPE_* returns from lambda. This differs from D's `scope(exit)` or Go's `defer`.

- **Complex cleanup logic:** If cleanup needs multiple steps with error handling, the lambda can become unwieldy. Consider a dedicated cleanup function.

- **C interop:** Guards are C++ objects. At C boundaries, must ensure cleanup runs before returning to C.

- **Debugging:** Stack traces show lambda locations, not logical cleanup names. Add comments for clarity.

---

## Read Next

- **User Manual - ScopeGuard** — Full API reference, all policies, all macros
- **Migration Guide - malloc/free to RAII** — Using unique_ptr and shared_ptr
- **Migration Guide - Error Handling to Expected** — Related error handling migration
- **Companion Guide - ScopeGuard** — Design rationale, policy design
- **Handbook - Exception Safety Guarantees** — Basic, strong, nothrow guarantees

---

*Migration Guide - Manual Resource Cleanup to ScopeGuard v1.0 — January 2025*
