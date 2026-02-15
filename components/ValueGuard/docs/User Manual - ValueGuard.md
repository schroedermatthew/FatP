---
doc_id: UM-VALUEGUARD-001
doc_type: "User Manual"
title: "ValueGuard"
fatp_components: ["ValueGuard"]
topics: ["RAII value restoration", "temporary mutation", "save-restore pattern", "scope-based value management", "exception-safe value changes"]
constraints: ["value copy cost", "destructor exception interaction", "nested guard ordering"]
cxx_standard: "C++20"
std_equivalent: null
boost_equivalent: null
build_modes: ["Debug", "Release"]
last_verified: "2026-02-15"
audience: ["C++ developers", "AI assistants"]
status: "reviewed"
---

# User Manual - ValueGuard

**Scope:** Complete usage guide for `fat_p::ValueGuard`: the save-modify-restore RAII pattern, construction options (save-only, save-and-modify), early restoration, integration with ScopeGuard, and exception safety guarantees.

**Not covered:**
- ScopeGuard (general cleanup actions; see ScopeGuard User Manual)
- Transaction rollback patterns beyond single-value restoration
- Thread-safe value guarding (ValueGuard is single-threaded)

**Prerequisites:** C++20; understanding of RAII and destructor-based cleanup; familiarity with the temporary-value-mutation pattern

---

## User Manual Card

**Component:** ValueGuard
**Primary use case:** Temporarily change a value and guarantee automatic restoration when the scope exits, regardless of exceptions or early returns
**Integration pattern:** Construct `ValueGuard guard(variable, newValue);` at the point where the temporary change is needed; the original value is restored automatically when `guard` goes out of scope
**Key API:** `ValueGuard<T>`, constructor (reference, optional new value), `.restore()` (early restoration), `.dismiss()` (cancel restoration)
**std equivalent:** None
**Common mistakes:** Creating a ValueGuard for a reference that outlives the guarded scope; dismissing a guard and forgetting to restore manually; guarding non-copyable types without move support
**Performance notes:** One copy on construction (save), one copy on destruction (restore). Zero overhead beyond the copies. See `components/ValueGuard/results/` for current data

---
## What is ValueGuard?

### The Problem: Temporary Value Mutations

Consider a common pattern in systems programming: you need to temporarily change a value, do some work, then restore the original—regardless of how the scope exits.

```cpp
// The naive approach: fragile and error-prone
void enable_debug_logging(Logger& logger) {
    LogLevel original = logger.level;
    logger.level = LogLevel::Debug;
    
    process_request();  // What if this throws?
    
    logger.level = original;  // Never reached on exception
}
```

This pattern appears everywhere: temporarily enabling debug mode, changing thread priority during critical sections, swapping file handles, or modifying configuration during tests. The naive approach fails because:

1. **Exceptions bypass restoration:** If `process_request()` throws, `logger.level` remains corrupted
2. **Early returns multiply restoration points:** Each `return` statement needs its own restoration
3. **Maintenance burden:** Adding new exit paths requires remembering to add restoration code

### The Solution: RAII Value Guarding

ValueGuard encapsulates the save-modify-restore pattern in a single RAII object:

```cpp
void enable_debug_logging(Logger& logger) {
    fat_p::ValueGuard guard(logger.level, LogLevel::Debug);
    
    process_request();  // Exception? Guard restores in destructor
    
}  // Normal exit? Guard restores in destructor
```

The guard captures the original value at construction, assigns the new value, and restores the original when it goes out of scope—no matter how.

### Where ValueGuard Fits in the C++ Landscape

**Standard library:** C++ has no direct equivalent. `std::optional` stores values but doesn't manage restoration. Smart pointers manage ownership, not temporary mutations.

**Boost:** `scope_exit` provides cleanup-on-exit but requires manual lambda construction for value restoration, without built-in original value storage or policy customization.

**ValueGuard's niche:** Policy-based value restoration with compile-time customization, move-only type support, and zero virtual dispatch overhead.

---

## Core Architecture

### Design Overview

```cpp
template <typename T, typename Policy = ValueGuardCopyPolicy<T>>
class [[nodiscard]] ValueGuard : private Policy {
    T* target_;      // Pointer to the guarded variable
    T original_;     // Captured original value
    bool active_;    // Whether restoration should occur
};
```

**Key design decisions:**

1. **Pointer, not reference:** Allows move assignment between guards (references can't be reseated)

2. **Private inheritance from Policy:** Enables Empty Base Optimization—stateless policies add zero bytes to the guard

3. **`[[nodiscard]]` attribute:** Prevents accidental immediate destruction:
   ```cpp
   ValueGuard(x, 42);  // Warning: temporary immediately destroyed
   auto guard = ValueGuard(x, 42);  // Correct: guard lives until scope exit
   ```

4. **Compile-time policy resolution:** No virtual dispatch, no runtime branching for policy selection

### Memory Layout

For stateless policies (most common case):

```
┌────────────────┬────────────────┬──────────┐
│    target_     │   original_    │  active_ │
│    (T*)        │     (T)        │  (bool)  │
│   8 bytes      │  sizeof(T)     │  1 byte  │
└────────────────┴────────────────┴──────────┘
```

For stateful policies (e.g., CustomPolicy with lambda):

```
┌────────────────┬────────────────┬──────────┬───────────────┐
│    target_     │   original_    │  active_ │  Policy state │
│    (T*)        │     (T)        │  (bool)  │  (varies)     │
└────────────────┴────────────────┴──────────┴───────────────┘
```

### Policy Architecture

Each policy must provide:

```cpp
struct SomePolicy {
    static constexpr bool is_nothrow_restore = /* noexcept specification */;
    
    void execute(T& target, T&& original) noexcept(is_nothrow_restore) {
        // Restoration logic
    }
};
```

The `is_nothrow_restore` constant enables the destructor to be conditionally `noexcept`, which affects exception safety and move semantics.

---

## Getting Started

### Prerequisites

- C++17 or later
- Header-only: just include `ValueGuard.h`
- Depends on: `FatPTypeTraits.h` (for `is_value_guard` trait)

### Integration

```cpp
#include "ValueGuard.h"
```

### First Program

```cpp
#include <iostream>
#include "ValueGuard.h"

int main() {
    int config_value = 100;
    std::cout << "Before: " << config_value << "\n";  // 100
    
    {
        fat_p::ValueGuard guard(config_value, 200);
        std::cout << "Inside: " << config_value << "\n";  // 200
    }
    
    std::cout << "After: " << config_value << "\n";  // 100
    return 0;
}
```

Output:
```
Before: 100
Inside: 200
After: 100
```

---

## Policies In Depth

### What is a Policy?

A policy controls **how** restoration occurs. ValueGuard separates the **what** (restore this variable) from the **how** (copy back, move back, conditionally restore, custom logic). This separation enables compile-time optimization and code reuse.

### CopyPolicy (Default for lvalue new_value)

**What:** Restores by copy-assignment: `target = original`

**When to use:** When you need to inspect the original value during the scope, or when the type is cheap to copy.

```cpp
std::string message = "Hello";
{
    fat_p::ValueGuard guard(message, std::string("Goodbye"));
    
    // Can inspect original during scope
    std::cout << "Original was: " << guard.original() << "\n";  // "Hello"
    std::cout << "Current is: " << message << "\n";              // "Goodbye"
}
// message == "Hello" (restored via copy)
```

**Deduction:** Triggered when new_value is an lvalue:
```cpp
const std::string new_val = "Goodbye";
ValueGuard guard(message, new_val);  // CopyPolicy deduced
```

### MovePolicy (Default for rvalue new_value)

**What:** Restores by move-assignment: `target = std::move(original)`

**When to use:** For move-only types, or when you don't need the original value to remain valid after restoration.

```cpp
std::unique_ptr<Resource> resource = std::make_unique<Resource>("primary");
{
    fat_p::ValueGuard<std::unique_ptr<Resource>, 
                      fat_p::ValueGuardMovePolicy<std::unique_ptr<Resource>>>
        guard(resource, std::make_unique<Resource>("temporary"));
    
    resource->use();  // Using temporary resource
}
// resource now owns "primary" again (restored via move)
```

**Deduction:** Triggered when new_value is an rvalue:
```cpp
ValueGuard guard(resource, std::make_unique<Resource>("temp"));  // MovePolicy deduced
```

### NoRestorePolicy

**What:** Does nothing on destruction. Equivalent to immediately calling `release()`.

**When to use:** When you always want to keep the mutation (but want consistent API with other guards).

```cpp
int counter = 0;
{
    auto guard = fat_p::make_value_guard_no_restore(counter, 42);
    // counter == 42
}
// counter == 42 (NOT restored)
```

### ConditionalPolicy

**What:** Restores only if a user-provided condition evaluates to `true` at scope exit.

**When to use:** Transaction patterns where restoration depends on success/failure.

```cpp
bool operation_succeeded = false;

{
    auto guard = fat_p::make_value_guard_conditional(
        database.transaction_id,
        new_transaction_id(),
        [&]() { return !operation_succeeded; }  // Restore only on failure
    );
    
    operation_succeeded = perform_database_operation();
}
// If operation_succeeded: new ID kept
// If !operation_succeeded: original ID restored
```

**Important:** The condition is evaluated in the destructor, so captured references must remain valid.

### CustomPolicy

**What:** Invokes a user-provided function with signature `void(T& target, T&& original)`.

**When to use:** Logging, metrics, complex restoration logic, or types requiring special handling.

```cpp
{
    auto guard = fat_p::make_value_guard_custom(
        connection_count,
        connection_count + 1,
        [](int& target, int&& original) {
            metrics::record("connection_restored", target, original);
            target = original;
        }
    );
    
    handle_connection();
}
// Custom restorer logs and restores
```

---

## Factory Functions

Factory functions provide explicit policy selection when CTAD (Class Template Argument Deduction) doesn't deduce what you want.

| Factory | Policy | Use Case |
|---------|--------|----------|
| `make_value_guard(target, new_val)` | CopyPolicy | Default copy-based restoration |
| `make_value_guard_move(target, rvalue)` | MovePolicy | Move-only types |
| `make_value_guard_no_restore(target, new_val)` | NoRestorePolicy | Always commit |
| `make_value_guard_conditional(target, rvalue, cond)` | ConditionalPolicy | Transaction patterns |
| `make_value_guard_custom(target, new_val, restorer)` | CustomPolicy | Custom restoration logic |

```cpp
// Equivalent declarations:
fat_p::ValueGuard<int, fat_p::ValueGuardCopyPolicy<int>> guard1(x, 42);
auto guard2 = fat_p::make_value_guard(x, 42);
```

---

## Deduction Guides

C++17 deduction guides enable concise syntax without explicit template arguments.

### Basic Deduction

```cpp
int x = 10;
const int y = 20;

ValueGuard g1(x, y);          // CopyPolicy (lvalue new_value)
ValueGuard g2(x, 30);         // MovePolicy (rvalue new_value)
```

### Callable Deduction

When a third argument is provided, deduction guides distinguish between:

- **Two-argument callable** `(T&, T&&) -> void` → CustomPolicy
- **Zero-argument callable** `() -> bool` → ConditionalPolicy

```cpp
// Custom restorer (takes target and original)
ValueGuard g3(x, 42, [](int& t, int&& o) { t = o + 1; });  // CustomPolicy

// Condition (takes nothing, returns bool)
ValueGuard g4(x, 42, []() { return true; });               // ConditionalPolicy
```

### Disambiguation Edge Case

If a functor satisfies **both** interfaces, neither deduction guide matches (mutual exclusion). Use factory functions:

```cpp
struct Ambiguous {
    bool operator()() const { return true; }
    void operator()(int&, int&&) const {}
};

// ValueGuard g(x, 42, Ambiguous{});  // ERROR: ambiguous

// Solution: use explicit factory
auto g = fat_p::make_value_guard_custom(x, 42, Ambiguous{});
```

---

## State Introspection

### Checking Guard Status

```cpp
fat_p::ValueGuard guard(value, 42);

if (guard.is_active()) {
    // Guard will restore on destruction
}

guard.release();

if (!guard.is_active()) {
    // Guard will NOT restore (released)
}
```

### Accessing Values

```cpp
int original_value = 10;
fat_p::ValueGuard guard(original_value, 42);

guard.original();  // Returns 10 (const T&)
guard.current();   // Returns 42 (T& to the guarded variable)
```

**Note:** `current()` returns a reference to the target variable, not a copy. Modifying `guard.current()` modifies the guarded variable:

```cpp
guard.current() = 100;  // original_value is now 100
```

---

## Move Semantics

### Moving Guards

ValueGuard supports move construction and move assignment, enabling storage in containers:

```cpp
std::vector<fat_p::ValueGuard<int, fat_p::ValueGuardCopyPolicy<int>>> guards;

int value = 0;
{
    fat_p::ValueGuard guard(value, 42);
    guards.push_back(std::move(guard));  // Guard transferred to vector
    
    // guard.is_active() == false (moved-from)
}
// value still 42 (guard is in the vector)

guards.clear();  // Guard destroyed, value restored to 0
```

### Move Assignment Restores First

When assigning a new guard to an existing guard, the existing guard's target is restored before the transfer:

```cpp
int a = 10, b = 20;
{
    fat_p::ValueGuard guard_a(a, 100);  // a = 100, will restore to 10
    fat_p::ValueGuard guard_b(b, 200);  // b = 200, will restore to 20
    
    guard_b = std::move(guard_a);       // b restored to 20 IMMEDIATELY
    
    // Now guard_b manages 'a', guard_a is inactive
}
// a restored to 10, b stays at 20
```

This ensures no guarded value is silently abandoned.

### Self-Move is No-Op

```cpp
guard = std::move(guard);  // Safe: guard unchanged
```

---

## Swap Operations

### Member Swap

```cpp
int a = 10, b = 20;
fat_p::ValueGuard guard_a(a, 100);
fat_p::ValueGuard guard_b(b, 200);

guard_a.swap(guard_b);

// Now guard_a manages 'b', guard_b manages 'a'
// Destruction order determines which is restored first
```

### ADL Swap

```cpp
using std::swap;
swap(guard_a, guard_b);  // Finds fat_p::swap via ADL
```

### Swap Exchanges ALL State

Swap exchanges `target_`, `original_`, and `active_`. If one guard was released, that release state transfers:

```cpp
guard_a.release();
guard_a.swap(guard_b);

// guard_a is now ACTIVE (was guard_b's state)
// guard_b is now INACTIVE (was guard_a's state)
```

---

## Exception Safety

### Strong Guarantee in Constructors

If the assignment of new_value throws, the target is restored to its original state:

```cpp
struct ThrowOnAssign {
    ThrowOnAssign& operator=(ThrowOnAssign&&) { 
        throw std::runtime_error("assign failed"); 
    }
};

ThrowOnAssign target;
try {
    fat_p::ValueGuard guard(target, ThrowOnAssign{});
} catch (...) {
    // target is in its original state, not moved-from
}
```

### Destructor noexcept Specification

The destructor is `noexcept` if and only if `Policy::is_nothrow_restore` is `true`:

```cpp
static_assert(noexcept(std::declval<
    fat_p::ValueGuard<int, fat_p::ValueGuardCopyPolicy<int>>>().~ValueGuard()));
```

**Warning:** If restoration can throw and an exception is already in flight, `std::terminate` is called per C++ rules. Design policies with `noexcept` restoration when possible.

---

## Performance Characteristics

### Complexity

| Operation | Complexity | Notes |
|-----------|------------|-------|
| Construction | O(1) + assignment | Stores pointer, copies/moves original |
| Destruction | O(1) + policy execution | Conditional restoration |
| `release()` | O(1) | Boolean flag |
| `is_active()` | O(1) | Boolean read |
| `original()` | O(1) | Reference return |
| `current()` | O(1) | Pointer dereference |
| `swap()` | O(1) + swap(T) | Member-wise swap |

### Zero Overhead Abstraction

- **No virtual dispatch:** Policy resolved at compile time
- **Empty Base Optimization:** Stateless policies add zero bytes
- **Inlining:** Simple policies inline completely
- **No heap allocation:** Stack-only storage

### Benchmark Guidance

```cpp
// Measure guard overhead vs. manual save/restore
auto guard_time = measure([&]() {
    fat_p::ValueGuard guard(value, new_value);
    DoNotOptimize(value);
});

auto manual_time = measure([&]() {
    auto original = value;
    value = new_value;
    DoNotOptimize(value);
    value = original;
});
```

Typical result: Guard overhead is ~1-3 nanoseconds for trivial types, dominated by the copy/move of the original value.

---

## Comparison with Alternatives

### vs. Manual Save/Restore

```cpp
// Manual: 3 restoration points, exception-unsafe
void manual_approach(Config& cfg) {
    auto orig = cfg.mode;
    cfg.mode = NEW_MODE;
    
    if (check1()) { cfg.mode = orig; return; }
    if (check2()) { cfg.mode = orig; return; }
    cfg.mode = orig;
}

// ValueGuard: 0 restoration points, exception-safe
void guard_approach(Config& cfg) {
    fat_p::ValueGuard guard(cfg.mode, NEW_MODE);
    
    if (check1()) return;
    if (check2()) return;
}
```

| Aspect | Manual | ValueGuard |
|--------|--------|------------|
| Exception safety | Requires try-catch | Automatic |
| Restoration points | N per exit path | 0 |
| Maintenance | High | Low |
| Compile-time checking | None | `[[nodiscard]]` warning |

### vs. `scope_exit`

```cpp
// scope_exit: Requires explicit lambda
void scope_exit_approach(Config& cfg) {
    auto orig = cfg.mode;
    cfg.mode = NEW_MODE;
    auto guard = std::experimental::scope_exit([&]() { cfg.mode = orig; });
}

// ValueGuard: Captures original automatically
void guard_approach(Config& cfg) {
    fat_p::ValueGuard guard(cfg.mode, NEW_MODE);
}
```

| Aspect | scope_exit | ValueGuard |
|--------|------------|------------|
| Original value storage | Manual capture | Built-in |
| Policy customization | Lambda-based | Compile-time policy |
| Release semantics | Must add flag | Built-in `release()` |
| Move-only type support | Complex | Native |

---

## Migration Guide

### From Manual Save/Restore

**Before:**
```cpp
void function(State& state) {
    auto original = state.value;
    state.value = new_value;
    
    // ... code that might throw or return early ...
    
    state.value = original;
}
```

**After:**
```cpp
void function(State& state) {
    fat_p::ValueGuard guard(state.value, new_value);
    
    // ... code that might throw or return early ...
}
```

### From Try-Catch Restoration

**Before:**
```cpp
void function(State& state) {
    auto original = state.value;
    state.value = new_value;
    
    try {
        risky_operation();
    } catch (...) {
        state.value = original;
        throw;
    }
    state.value = original;
}
```

**After:**
```cpp
void function(State& state) {
    fat_p::ValueGuard guard(state.value, new_value);
    risky_operation();
}
```

### Incremental Adoption

1. **Start with high-risk code:** Functions with exceptions or multiple return paths
2. **Identify save/restore patterns:** Search for `auto original = ...` followed by restoration
3. **Replace one at a time:** Each replacement is isolated; no global changes required
4. **Add `[[nodiscard]]` checking:** Enable compiler warnings to catch unused guards

---

## Best Practices

### When to Use ValueGuard

- Temporary configuration changes during tests
- Debug mode toggles
- Thread priority adjustments during critical sections
- Transaction-style operations with rollback
- Recursive algorithms with state stacks

### When NOT to Use ValueGuard

- **Ownership transfer:** Use `std::unique_ptr` or `std::optional`
- **Simple, single-exit functions:** Manual approach may be clearer
- **Non-assignable types:** ValueGuard requires `operator=`

### Naming Conventions

```cpp
// Good: Descriptive guard names
fat_p::ValueGuard debug_mode_guard(config.debug, true);
fat_p::ValueGuard priority_guard(thread.priority, HIGH);

// Avoid: Generic names that don't convey purpose
fat_p::ValueGuard g(x, y);
```

### Scope Minimization

```cpp
// Good: Guard scope matches mutation scope
void function() {
    // ... unrelated code ...
    {
        fat_p::ValueGuard guard(config.mode, SPECIAL_MODE);
        special_mode_operation();
    }
    // ... unrelated code ...
}

// Avoid: Guard scope larger than necessary
void function() {
    fat_p::ValueGuard guard(config.mode, SPECIAL_MODE);
    // ... unrelated code that doesn't need special mode ...
    special_mode_operation();
    // ... more unrelated code ...
}
```

---

## Troubleshooting

### Compilation Errors

**Error:** `no matching constructor for initialization of 'ValueGuard'`

**Cause:** Type `T` is not assignable.

**Solution:** Ensure `T` has `operator=` defined. For move-only types, use MovePolicy.

---

**Error:** `static assertion failed: T must be copy-constructible for CopyPolicy`

**Cause:** Using CopyPolicy with a move-only type.

**Solution:** Use MovePolicy explicitly:
```cpp
fat_p::ValueGuard<MoveOnlyType, fat_p::ValueGuardMovePolicy<MoveOnlyType>> guard(...);
```

---

**Warning:** `ignoring return value of function declared with 'nodiscard' attribute`

**Cause:** Guard temporary is immediately destroyed:
```cpp
fat_p::ValueGuard(x, 42);  // Warning: no effect
```

**Solution:** Store the guard:
```cpp
auto guard = fat_p::ValueGuard(x, 42);
```

---

### Runtime Issues

**Issue:** Value not restored after scope exit

**Possible causes:**
1. Guard was moved from: `std::move(guard)` transfers ownership
2. `release()` was called: Guard won't restore
3. Destructor threw: If `is_nothrow_restore` is false and exception in flight

**Diagnosis:** Check `guard.is_active()` before scope exit.

---

**Issue:** Unexpected value after restoration

**Possible cause:** Using CopyPolicy but original was modified:
```cpp
fat_p::ValueGuard guard(x, 42);
const_cast<int&>(guard.original()) = 999;  // DON'T DO THIS
```

**Solution:** Don't modify the captured original. It's returned as `const T&` for a reason.

---

## Summary

### Key Features

| Feature | Benefit |
|---------|---------|
| RAII restoration | Automatic, exception-safe cleanup |
| Policy-based design | Compile-time customization |
| 5 built-in policies | Copy, Move, NoRestore, Conditional, Custom |
| Deduction guides | Concise C++17 syntax |
| Move semantics | Container storage, ownership transfer |
| State introspection | `original()`, `current()`, `is_active()` |

### Quick Start

```cpp
#include "ValueGuard.h"

int main() {
    int value = 10;
    {
        fat_p::ValueGuard guard(value, 42);
        // value == 42
    }
    // value == 10 (restored)
}
```

### Related Components

- **ScopeGuard:** General-purpose scope-exit actions
- **Expected:** Error handling with value-or-error semantics
- **Signal:** Event system that may temporarily modify slot state

---

*ValueGuard.h — Fat-P Library v2.4*
