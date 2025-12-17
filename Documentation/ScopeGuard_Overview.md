# ScopeGuard: A Fat-P Library Showcase

## Executive Summary

ScopeGuard is a **policy-based RAII cleanup system** that executes arbitrary cleanup code when a scope exits—unconditionally, only on success, or only on failure. Unlike manual try/catch/finally patterns (verbose, error-prone) or `std::unique_ptr` with custom deleters (limited to pointer cleanup), ScopeGuard handles **any cleanup action** with dismissible execution and optional exception-based conditional logic. The `[[nodiscard]]` enforcement and move-only semantics prevent the common bugs that plague manual cleanup code.

---

## The Problem Domain

### What Goes Wrong Without It

```cpp
// The resource leak minefield
void process() {
    FILE* file = fopen("data.txt", "r");
    if (!file) return;
    
    char* buffer = malloc(1024);
    if (!buffer) {
        fclose(file);  // Remember to close!
        return;
    }
    
    if (!read_data(file, buffer)) {
        free(buffer);   // Remember to free!
        fclose(file);   // Remember to close!
        return;
    }
    
    if (!process_data(buffer)) {
        free(buffer);   // Duplicated cleanup
        fclose(file);   // Duplicated cleanup
        return;
    }
    
    free(buffer);       // Duplicated cleanup
    fclose(file);       // Duplicated cleanup
}
// Every return path needs cleanup. Easy to forget. Easy to mess up order.

// The exception-unsafe pattern
void transfer(Account& from, Account& to, int amount) {
    from.withdraw(amount);  // What if to.deposit throws?
    to.deposit(amount);      // from.withdraw already happened!
    // No rollback mechanism
}
```

| Issue | HPC Impact |
|-------|------------|
| Duplicated cleanup | Every return path needs same cleanup code |
| Forgotten cleanup | Memory/resource leaks on error paths |
| Exception unsafety | Partial operations leave inconsistent state |
| Order sensitivity | Cleanup must happen in reverse acquisition order |

### The Standard's Limitation

C++ RAII handles objects with destructors, but:
- **Ad-hoc cleanup** requires custom wrapper classes
- **Conditional cleanup** (success/failure only) requires manual tracking
- **Rollback semantics** require explicit state management
- **`finally`** doesn't exist in C++

`std::unique_ptr` with custom deleter handles pointer cleanup but not arbitrary actions.

---

## Architecture: Policy-Based Cleanup Execution

### The Mechanism: Conditional Destruction

```cpp
template<typename Cleanup, typename Policy = ScopeGuardAlwaysPolicy>
class ScopeGuard {
    Cleanup cleanup_;
    bool dismissed_ = false;
    
public:
    ~ScopeGuard() {
        if (!dismissed_ && Policy::should_execute()) {
            cleanup_();
        }
    }
    
    void dismiss() noexcept { dismissed_ = true; }
};

// Policies
struct ScopeGuardAlwaysPolicy {
    static constexpr bool should_execute() { return true; }
};

struct ScopeGuardSuccessPolicy {
    static bool should_execute() { 
        return std::uncaught_exceptions() == 0; 
    }
};

struct ScopeGuardFailurePolicy {
    static bool should_execute() { 
        return std::uncaught_exceptions() > 0; 
    }
};
```

**Why `std::uncaught_exceptions()` (C++17):**

Unlike C++11's `std::uncaught_exception()` (boolean), C++17's version returns a count, enabling correct behavior when guards are nested during stack unwinding.

### Factory Functions

```cpp
auto guard = makeScopeGuard([&] { cleanup(); });     // Always executes
auto success = makeScopeSuccess([&] { commit(); });   // Only on success
auto failure = makeScopeFailure([&] { rollback(); }); // Only on exception
```

---

## Feature Inventory

### 1. Unconditional Cleanup (ScopeGuard)

```cpp
void process() {
    FILE* file = fopen("data.txt", "r");
    if (!file) return;
    auto guard = makeScopeGuard([&] { fclose(file); });
    
    // ... any number of return paths ...
    // ... any exceptions ...
    // file is ALWAYS closed when scope exits
}
```

**Mechanism:** Destructor always calls cleanup unless `dismiss()` was called.

### 2. Success-Only Execution (ScopeSuccess)

```cpp
void commit_transaction(Database& db) {
    db.begin();
    auto commit = makeScopeSuccess([&] { 
        db.commit(); 
        log("Transaction committed");
    });
    
    // ... operations that might throw ...
    
    // If no exception: commit() runs
    // If exception thrown: commit() does NOT run
}
```

**Mechanism:** `std::uncaught_exceptions() == 0` at destruction time.

### 3. Failure-Only Execution (ScopeFailure)

```cpp
void transfer(Account& from, Account& to, int amount) {
    from.withdraw(amount);
    auto rollback = makeScopeFailure([&] {
        from.deposit(amount);  // Undo the withdrawal
    });
    
    to.deposit(amount);  // If this throws, withdrawal is rolled back
    
    rollback.dismiss();  // Success: don't rollback
}
```

**Mechanism:** `std::uncaught_exceptions() > 0` at destruction time.

### 4. Dismissible Guards

```cpp
auto guard = makeScopeGuard([&] { delete resource; });

// ... operations ...

if (transfer_ownership) {
    guard.dismiss();  // Don't delete—ownership transferred
}
// If not dismissed, cleanup runs at scope exit
```

### 5. [[nodiscard]] Protection

```cpp
// WRONG: Guard immediately destroyed, cleanup runs now!
makeScopeGuard([&] { fclose(file); });  // Warning/error: discarded [[nodiscard]]

// CORRECT: Guard lives until scope exit
auto guard = makeScopeGuard([&] { fclose(file); });
```

**Mechanism:** `[[nodiscard]]` on factory function return type forces variable capture.

### 6. Move-Only Semantics

```cpp
auto guard = makeScopeGuard([&] { cleanup(); });
auto moved = std::move(guard);  // Ownership transferred
// guard is now dismissed (won't cleanup)
// moved will cleanup
```

**Mechanism:** Move constructor dismisses source, transfers cleanup to destination.

### 7. Nested Guards with Correct Unwinding

```cpp
void complex_operation() {
    Resource a = acquire_a();
    auto ga = makeScopeGuard([&] { release_a(a); });
    
    Resource b = acquire_b();
    auto gb = makeScopeGuard([&] { release_b(b); });
    
    Resource c = acquire_c();
    auto gc = makeScopeGuard([&] { release_c(c); });
    
    // On exit (normal or exception):
    // gc runs first (release_c)
    // gb runs second (release_b)
    // ga runs last (release_a)
    // Correct reverse order guaranteed by stack destruction order
}
```

---

## Why Not Alternatives?

| If You Need... | Why Not try/catch | Why Not unique_ptr | Why Not Boost.ScopeExit | Fat-P Advantage |
|----------------|-------------------|-------------------|------------------------|-----------------|
| Arbitrary cleanup | ✅ Works but verbose | ❌ Pointers only | ✅ Works | ✅ Clean lambda syntax |
| Success/failure only | ❌ Manual tracking | ❌ Not supported | Partial | ✅ Three policies |
| [[nodiscard]] | ❌ No protection | ❌ No protection | ❌ No protection | ✅ Enforced |
| Zero dependencies | ✅ Works | ✅ Standard | ❌ Requires Boost | ✅ Single header |
| Correct exception count | N/A | N/A | ❌ C++11 only | ✅ C++17 uncaught_exceptions |

**The Sweet Spot:** ScopeGuard is the only option combining success/failure policies, [[nodiscard]] protection, correct C++17 exception counting, and zero dependencies.

---

## The "Forever Stuck" Reality

**Standard Reality:** C++ will likely never add `finally` or `defer`:
- Conflicts with RAII philosophy
- Committee prefers destructor-based cleanup
- Proposals have repeatedly stalled

ScopeGuard provides `finally`/`defer` semantics through RAII, aligning with C++ philosophy while offering the convenience other languages provide.

---

## Performance Characteristics

| Scenario | Cost | Notes |
|----------|------|-------|
| Guard creation | ~0 ns | Lambda capture, no allocation |
| Guard destruction (executes) | Cleanup cost | Direct function call |
| Guard destruction (dismissed) | ~0 ns | Boolean check only |
| `std::uncaught_exceptions()` | ~1-2 ns | Thread-local counter read |

**Compiler Optimization:**

```cpp
auto guard = makeScopeGuard([&] { fclose(file); });
// Compiles to essentially:
// (at scope exit) if (!dismissed) fclose(file);
// No heap allocation, no virtual call
```

### Where Fat-P Wins
- Complex resource management with multiple cleanup actions
- Transaction-style operations needing rollback
- APIs returning resources that need conditional cleanup

### Where Fat-P Loses (Honesty Builds Trust)
- Single resource → `std::unique_ptr` is simpler
- No conditional execution needed → simple RAII wrapper class suffices
- Performance-critical hot paths → inline cleanup may be faster

---

## Integration Points

```
ScopeGuard.h
    ↓ components
ScopeGuardPolicies.h    (Always, Success, Failure policies)
    ↓ used by
SmallVector.h           (transactional reallocation)
IdGenerator.h           (IdGuard implementation)
Database transactions   (commit/rollback patterns)
File operations         (guaranteed close)
```

---

## Final Assessment

ScopeGuard delivers on the fat_p promise through three pillars:

### 1. Permanence
C++ will never add `finally` or `defer`—the committee prefers RAII. ScopeGuard provides these semantics permanently through RAII-compatible design.

### 2. Specialization
Three policies (Always, Success, Failure) handle different cleanup scenarios. `std::uncaught_exceptions()` enables correct behavior during nested exception handling. [[nodiscard]] prevents the common "accidentally destroyed" bug.

### 3. Control
Dismissible execution lets you cancel cleanup when ownership transfers. Move semantics enable guard transfer. Lambda capture handles any cleanup action, not just pointer deletion.

**Architectural Verdict:** ScopeGuard transforms cleanup from **duplicated try/catch blocks** to **declarative scope-exit actions**. It's `defer` for C++, done with RAII.

---

*ScopeGuard.h — Fat-P Library*
