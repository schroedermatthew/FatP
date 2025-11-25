# ScopeGuard User Manual

**Version:** 2.0  
**Library:** fat_p C++ Utilities  
**Standard:** C++17  
**Type:** Header-only

---

## Table of Contents

1. [Overview](#overview)
2. [Quick Start](#quick-start)
3. [Core API](#core-api)
4. [Policies](#policies)
5. [Macros](#macros)
6. [Advanced Usage](#advanced-usage)
7. [Best Practices](#best-practices)

---

## Overview

ScopeGuard provides RAII-based scope-exit cleanup with policy-based exception handling. Execute cleanup code when leaving a scope, regardless of how (normal return, exception, early exit).

### Include

```cpp
#include "ScopeGuard.h"
using namespace fat_p;
```

### Basic Concept

```cpp
void process_file() {
    FILE* file = fopen("data.txt", "r");
    auto guard = make_scope_guard([&] { fclose(file); });
    
    // ... use file ...
    
    if (error_condition) return;  // fclose still called
    
    // ... more processing ...
    
}  // fclose called here too
```

---

## Quick Start

### Simple Cleanup

```cpp
void example() {
    auto* resource = acquire_resource();
    SCOPE_EXIT { release_resource(resource); };
    
    use_resource(resource);
    // Automatically released when scope exits
}
```

### Conditional Execution

```cpp
void transaction() {
    begin_transaction();
    
    SCOPE_FAIL { rollback_transaction(); };   // Only on exception
    SCOPE_SUCCESS { commit_transaction(); };  // Only on normal exit
    
    do_work();
}
```

### Manual Control

```cpp
void maybe_cleanup() {
    auto guard = make_scope_guard([] { cleanup(); });
    
    if (success) {
        guard.release();  // Don't execute cleanup
    }
    // Otherwise cleanup runs
}
```

---

## Core API

### make_scope_guard

Creates a scope guard with the given cleanup action.

```cpp
template <typename F>
auto make_scope_guard(F&& action);

// Usage
auto guard = make_scope_guard([] { cleanup(); });
auto guard2 = make_scope_guard([&] { delete ptr; });
```

### ScopeGuard Class

```cpp
template <typename F, 
          typename ThrowingPolicy = ScopeGuardTerminatePolicy,
          template<typename> class ActionPolicy = DefaultActionPolicy>
class ScopeGuard {
public:
    // Construct with action
    explicit ScopeGuard(F&& action);
    
    // Move-only
    ScopeGuard(ScopeGuard&& other);
    ScopeGuard& operator=(ScopeGuard&&);
    
    // Destructor executes action (if not released)
    ~ScopeGuard();
    
    // Prevent action execution
    void release() noexcept;
    
    // Check if action will execute
    bool is_active() const noexcept;
};
```

### release()

Disables the cleanup action.

```cpp
auto guard = make_scope_guard([] { cleanup(); });

if (transfer_ownership()) {
    guard.release();  // New owner handles cleanup
}
// Otherwise guard cleans up
```

---

## Policies

### ScopeGuardTerminatePolicy (Default)

Calls `std::terminate()` if cleanup throws.

```cpp
auto guard = make_scope_guard<ScopeGuardTerminatePolicy>(
    [] { might_throw(); }
);
// If might_throw() throws in destructor → terminate()
```

### ScopeGuardNothrowPolicy

Requires noexcept action (compile-time check).

```cpp
auto guard = make_scope_guard<ScopeGuardNothrowPolicy>(
    []() noexcept { safe_cleanup(); }
);
// Compile error if action might throw
```

### ScopeGuardLogAndSwallowPolicy

Logs exception and continues.

```cpp
auto guard = make_scope_guard<ScopeGuardLogAndSwallowPolicy>(
    [] { risky_cleanup(); }
);
// If throws: logs to cerr, continues execution
```

### ScopeGuardRethrowPolicy

Re-throws exceptions (use with caution).

```cpp
// Only use outside destructors!
auto guard = make_scope_guard<ScopeGuardRethrowPolicy>(
    [] { cleanup_that_might_fail(); }
);
```

---

## Macros

### SCOPE_EXIT

Execute on any scope exit.

```cpp
void func() {
    SCOPE_EXIT { cleanup(); };
    
    // ... code ...
}  // cleanup() always called
```

### SCOPE_FAIL

Execute only when exiting via exception.

```cpp
void func() {
    SCOPE_FAIL { rollback(); };
    
    risky_operation();  // If throws, rollback() called
    
    // Normal exit: rollback() NOT called
}
```

### SCOPE_SUCCESS

Execute only on normal (non-exception) exit.

```cpp
void func() {
    SCOPE_SUCCESS { finalize(); };
    
    work();
    
    // finalize() called here (no exception)
}
```

### How SCOPE_FAIL/SUCCESS Work

Uses `std::uncaught_exceptions()` to detect exception state:

```cpp
// Simplified implementation
~ScopeGuardOnFail() {
    if (std::uncaught_exceptions() > exceptions_at_construction_) {
        action_();  // Exception in flight
    }
}
```

---

## Advanced Usage

### Nested Guards

```cpp
void complex_operation() {
    Resource1* r1 = acquire1();
    SCOPE_EXIT { release1(r1); };
    
    Resource2* r2 = acquire2();
    SCOPE_EXIT { release2(r2); };
    
    Resource3* r3 = acquire3();
    SCOPE_EXIT { release3(r3); };
    
    use_all(r1, r2, r3);
    
}  // Released in reverse order: r3, r2, r1
```

### Transaction Pattern

```cpp
void transfer_money(Account& from, Account& to, Money amount) {
    from.withdraw(amount);
    SCOPE_FAIL { from.deposit(amount); };  // Rollback on failure
    
    to.deposit(amount);  // May throw
}
```

### Resource Transfer

```cpp
unique_ptr<Resource> create_or_throw() {
    auto* raw = new Resource();
    auto guard = make_scope_guard([&] { delete raw; });
    
    raw->initialize();  // May throw
    raw->validate();    // May throw
    
    guard.release();    // Success - transfer ownership
    return unique_ptr<Resource>(raw);
}
```

### Combining with Expected

```cpp
Expected<Result, Error> safe_operation() {
    auto* resource = acquire();
    SCOPE_EXIT { release(resource); };
    
    auto step1 = do_step1(resource);
    if (!step1) return step1.error();
    
    auto step2 = do_step2(resource);
    if (!step2) return step2.error();
    
    return Result{*step1, *step2};
}
```

### Custom Action Policy

```cpp
template <typename F>
struct PooledActionPolicy {
    F action;
    bool* pool_flag;
    
    void operator()() {
        action();
        *pool_flag = false;  // Return to pool
    }
};
```

---

## Best Practices

### Do

```cpp
// ✅ Use for resource cleanup
FILE* f = fopen(path, "r");
SCOPE_EXIT { if (f) fclose(f); };

// ✅ Use SCOPE_FAIL for rollback
SCOPE_FAIL { undo_changes(); };

// ✅ Use release() for ownership transfer
auto guard = make_scope_guard([&]{ delete ptr; });
new_owner.take(ptr);
guard.release();

// ✅ Keep cleanup actions simple
SCOPE_EXIT { counter--; };  // Simple, fast

// ✅ Use noexcept cleanup in destructors
SCOPE_EXIT { safe_cleanup(); };  // safe_cleanup() is noexcept
```

### Don't

```cpp
// ❌ Don't throw in cleanup (use noexcept)
SCOPE_EXIT { throw Error(); };  // Bad!

// ❌ Don't do heavy work in guards
SCOPE_EXIT { 
    log_to_database();    // I/O in cleanup
    send_notification();  // Network in cleanup
};

// ❌ Don't forget release() when transferring
auto guard = make_scope_guard([&]{ delete ptr; });
return ptr;  // Double-delete! Forgot release()

// ❌ Don't rely on execution order across guards
// Order is LIFO (last guard first), but don't write
// code that depends on subtle ordering
```

---

## Exception Safety in Destructors

### The Problem

```cpp
~MyClass() {
    cleanup1();  // Might throw
    cleanup2();  // What if cleanup1 threw?
}
```

### The Solution

```cpp
~MyClass() noexcept {
    auto guard1 = make_scope_guard<ScopeGuardLogAndSwallowPolicy>(
        [&] { cleanup1(); }
    );
    auto guard2 = make_scope_guard<ScopeGuardLogAndSwallowPolicy>(
        [&] { cleanup2(); }
    );
    // Both cleanups attempted, exceptions logged not propagated
}
```

---

## Performance

| Operation | Cost |
|-----------|------|
| Construction | ~0ns (inlined) |
| Destruction (action runs) | action cost only |
| release() | ~0ns |
| Macro overhead | ~0ns (optimized away) |

The guard itself has virtually zero overhead - you pay only for the cleanup action.

---

## Related Components

- **ScopeGuardPolicies.h**: Policy definitions
- **enforce.h**: Contract enforcement (complementary)
- **Expected.h**: Error handling without exceptions

---

**Document Version:** 1.0  
**Last Updated:** November 2025
