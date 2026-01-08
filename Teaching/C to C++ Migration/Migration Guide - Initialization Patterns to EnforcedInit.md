---
doc_id: MG-ENFORCEDINIT-001
doc_type: "Migration Guide"
title: "Initialization Flag Patterns to EnforcedInit"
fatp_components: ["EnforcedInit"]
topics: ["C migration", "uninitialized memory", "initialization patterns", "type safety", "isInit flags", "two-phase initialization"]
constraints: ["manual discipline at scale", "tribal knowledge invariants", "partial construction visibility", "debug-only validation"]
cxx_standard: "C++17"
last_verified: "2025-01-07"
audience: ["C developers", "migration teams", "AI assistants"]
status: "draft"
---

# Migration Guide - Initialization Flag Patterns to EnforcedInit

## Scope

This document shows how to migrate four related C initialization patterns to type-safe C++ using `EnforcedInit<T>`. It uses SQLite—the most deployed database engine in the world—as the case study, demonstrating that even extraordinary discipline cannot fully prevent initialization bugs.

## Not Covered

- Full `EnforcedInit<T>` API reference (see User Manual - EnforcedInit)
- Template implementation details (see Companion Guide - EnforcedInit)
- Other Fat-P enforcement components (`enforce.h`, `Expected<T>`)
- General C-to-C++ migration strategy (see Handbook - C++ Design Goals and Migration)
- Concurrency considerations for initialization

## Prerequisites

- Familiarity with C memory allocation patterns (`malloc`, `memset`, `calloc`)
- Understanding of C++ templates (basic level)
- Awareness of undefined behavior consequences
- Access to Fat-P headers

---

## Migration Guide Card

**C Pattern:** Initialization tracking via flags, state constants, magic numbers, and defensive zeroing  
**Why it fails:** Compiler and type system cannot enforce initialization order; discipline fails at scale  
**C++ Solution:** `EnforcedInit<T>` — type wrapper that enforces init-before-use at compile time or runtime  
**Migration effort:** Low to Medium — mechanical transformation of struct fields  
**Verification method:** Compile-time errors for missing `init()` calls; runtime exceptions in debug builds  
**Incremental migration:** Yes — can migrate one struct at a time; mixed old/new patterns coexist  
**Prerequisites:** None (leaf component)

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

Four related patterns that C programmers use to track initialization state. All share a common weakness: the type system provides no protection.

### Pattern 1: The isInit Flag

**Source:** [SQLite `src/global.c`](https://github.com/sqlite/sqlite/blob/master/src/global.c) and [`src/main.c`](https://github.com/sqlite/sqlite/blob/master/src/main.c)

A boolean flag tracks whether a struct's fields are valid:

```c
/* [Excerpt from src/global.c] */
SQLITE_WSD struct Sqlite3Config sqlite3Config = {
  SQLITE_DEFAULT_MEMSTATUS,   /* bMemstat */
  1,                          /* bCoreMutex */
  SQLITE_THREADSAFE==1,       /* bFullMutex */
  SQLITE_USE_URI,             /* bOpenUri */
  /* ... dozens more configuration fields ... */
  0,                          /* isInit - NOT YET INITIALIZED */
  0,                          /* inProgress */
  0,                          /* isMutexInit */
  /* ... */
};
```

Every access site must check the flag:

```c
/* [Excerpt from src/main.c] */
int sqlite3_initialize(void){
  /* If already initialized, this should be a no-op */
  if( sqlite3GlobalConfig.isInit ){
    sqlite3MemoryBarrier();
    return SQLITE_OK;
  }
  
  /* ... 50+ lines of initialization ... */
  
  sqlite3GlobalConfig.isInit = 1;  /* Mark as initialized */
  return SQLITE_OK;
}
```

And configuration changes must be rejected after initialization:

```c
/* [Excerpt from src/main.c] */
int sqlite3_config(int op, ...){
  /* sqlite3_config() shall return SQLITE_MISUSE if invoked while
  ** the SQLite library is in use. */
  if( sqlite3GlobalConfig.isInit ) return SQLITE_MISUSE_BKPT;
  /* ... */
}
```

**Why programmers use it:** Legitimate need to configure a subsystem before use, then lock it down.

---

### Pattern 2: State Constants and Magic Numbers

**Source:** [SQLite `src/vdbeInt.h`](https://github.com/sqlite/sqlite/blob/master/src/vdbeInt.h) and [`src/vdbe.c`](https://github.com/sqlite/sqlite/blob/master/src/vdbe.c)

State constants track object lifecycle:

```c
/* [Excerpt from src/vdbeInt.h] */
#define VDBE_INIT_STATE   0   /* Prepared statement under construction */
#define VDBE_READY_STATE  1   /* Ready to run but not yet started */
#define VDBE_RUN_STATE    2   /* Run in progress */
#define VDBE_HALT_STATE   3   /* Finished. Need reset() or finalize() */
```

Magic numbers provide sanity checks for sub-structures:

```c
/* [Excerpt from src/vdbeInt.h] */
struct VdbeFrame {
  Vdbe *v;                /* VM this frame belongs to */
  VdbeFrame *pParent;     /* Parent of this frame */
  Op *aOp;                /* Program instructions for parent frame */
  Mem *aMem;              /* Array of memory cells for parent frame */
#if SQLITE_DEBUG
  u32 iFrameMagic;        /* magic number for sanity checking */
#endif
  /* ... */
};

#define SQLITE_FRAME_MAGIC 0x879fb71e
```

The execution loop checks state before running:

```c
/* [Excerpt from src/vdbe.c] */
int sqlite3VdbeExec(Vdbe *p){
  assert( p->eVdbeState==VDBE_RUN_STATE );  /* Must be in run state */
  sqlite3VdbeEnter(p);
  /* ... execute the program ... */
}
```

**Why programmers use it:** Legitimate need to track object lifecycle and catch corruption.

---

### Pattern 3: Two-Phase Initialization

**Source:** [SQLite `src/vdbeaux.c`](https://github.com/sqlite/sqlite/blob/master/src/vdbeaux.c)

Objects are allocated first, then configured later:

```c
/* [Excerpt from src/vdbeaux.c] */
Vdbe *sqlite3VdbeCreate(Parse *pParse){
  sqlite3 *db = pParse->db;
  Vdbe *p;
  
  p = sqlite3DbMallocRawNN(db, sizeof(Vdbe));  /* Phase 1: Allocate */
  if( p==0 ) return 0;
  
  memset(&p->aOp, 0, sizeof(Vdbe)-offsetof(Vdbe,aOp));
  p->db = db;
  p->eVdbeState = VDBE_INIT_STATE;  /* Not ready to run yet */
  /* ... partial setup ... */
  
  return p;  /* Returned before fully initialized! */
}
```

A second function completes initialization:

```c
/* [Excerpt from src/vdbeaux.c] */
void sqlite3VdbeMakeReady(Vdbe *p, Parse *pParse){
  /* Phase 2: Allocate aMem, aOp arrays, set up cursors */
  /* ... extensive setup ... */
  p->eVdbeState = VDBE_READY_STATE;  /* NOW it's ready */
}
```

**Why programmers use it:** Legitimate need for deferred initialization when full configuration isn't known at allocation time.

---

### Pattern 4: Defensive memset

**Source:** [SQLite `src/malloc.c`](https://github.com/sqlite/sqlite/blob/master/src/malloc.c)

All allocations are zeroed to make uninitialized-value bugs more reproducible:

```c
/* [Excerpt from src/malloc.c] */
void *sqlite3MallocZero(u64 n){
  void *p = sqlite3Malloc(n);
  if( p ){
    memset(p, 0, (size_t)n);
  }
  return p;
}

void *sqlite3DbMallocZero(sqlite3 *db, u64 n){
  void *p;
  p = sqlite3DbMallocRaw(db, n);
  if( p ) memset(p, 0, (size_t)n);
  return p;
}
```

Debug builds fill memory with nonsense patterns:

```c
/* [Representative - from SQLite MEMDEBUG documentation] */
/* "The heavy wrapper fills each new allocation with nonsense data
**  prior to returning the allocation to the caller." */
```

**Why programmers use it:** Legitimate defensive measure to make bugs more reproducible and catch uninitialized reads in debug builds.

---

### The Common Thread

All four patterns share a fatal weakness: **the type system cannot enforce them**.

| Pattern | Enforcement Mechanism | Weakness |
|---------|----------------------|----------|
| isInit flag | Manual check at each use site | Forgettable |
| State constants | Manual check, often `#ifdef DEBUG` | Compiled out in release |
| Two-phase init | Caller must know to call phase 2 | No compiler error if forgotten |
| Defensive memset | Runtime behavior | Masks bugs instead of catching them |

The invariant "this field is only valid after initialization" exists only in documentation and programmer memory. The compiler sees a valid struct with accessible fields.

---

## Why They Fail

### The Hidden Assumption

All four patterns assume: **Every developer who touches this code will remember to check initialization state before access, forever, in every code path.**

This assumption fails because:
- New team members don't know the conventions
- Code paths multiply (error handling, edge cases, refactoring)
- Copy-paste propagates incomplete patterns
- Fatigue degrades discipline over time

### Pattern 1 Failure: Forgotten isInit Check

**Scenario:** A new function accesses `sqlite3Config.bMemstat` without checking `isInit`.

**Trigger:** Function called during early startup before `sqlite3_initialize()` completes.

**Result:** Reads uninitialized or partially-initialized value. Behavior depends on what happened to be in memory.

### Pattern 2 Failure: Debug-Only Validation

**Scenario:** Magic number check exists but is wrapped in `#if SQLITE_DEBUG`.

**Trigger:** Corruption occurs in release build where check is compiled out.

**Result:** Silent memory corruption. The sanity check that would have caught it doesn't exist in the deployed binary.

### Pattern 3 Failure: Partial Construction Escapes

**Scenario:** Code path uses `Vdbe*` after `sqlite3VdbeCreate()` but before `sqlite3VdbeMakeReady()`.

**Trigger:** Error handling path, early return, or exception between the two calls.

**Result:** Access to `p->aMem` causes undefined behavior—the array hasn't been allocated yet.

### Pattern 4 Failure: Zero Is a Valid Value

**Scenario:** Field is zeroed by `memset`, but zero is a legitimate value (not a sentinel).

**Trigger:** Code assumes "zero means uninitialized" but zero is actually valid input.

**Result:** Logic error where the code treats valid data as uninitialized, or uninitialized data as valid.

---

### Evidence

#### CVE-2020-11655: AggInfo Initialization Mishandling

**Fact:** A malformed window-function query caused a segmentation fault because the `AggInfo` object's initialization was mishandled. The fix required ensuring proper initialization ordering.

**Source:** [SQLite CVE-2020-11655](https://www.sqlite.org/cves.html)

#### CVE-2025-6965: Aggregate Terms Exceeding Columns  

**Fact:** Memory corruption where aggregate term count could exceed available columns during initialization of aggregate processing structures.

**Source:** [SQLite Security Advisory](https://www.sqlite.org/cves.html)

#### Static Analysis Findings

**Fact:** Static analyzers regularly flag SQLite code for potential uninitialized access:

```c
/* [Representative - pattern reported by static analyzers] */
for(j=0; j<nKeyword; j++){
  h = aKeywordTable[j].hash % i;
  aKWHash[h] *= 2;    /* flagged: aKWHash[h] may be uninitialized */
  aKWHash[h]++;
}
```

SQLite developers often respond that these are false positives (array zeroed by `calloc`), but the pattern illustrates the problem: **the type doesn't communicate initialization requirements**.

#### Changelog Evidence

**Fact:** SQLite release notes contain recurring initialization fixes:

- "Fixed uninitialized variable in fts5"
- "Ensure pCsr->aBuffer is initialized before use"
- "Initialize Parse.zErrMsg to NULL"
- "Fix potential read of uninitialized memory in ALTER TABLE"

These aren't failures of intelligence—they're failures of **manual discipline at scale**. Every one passed code review by experts.

---

### The Cost

| Failure Type | Consequence |
|--------------|-------------|
| Read uninitialized memory | Undefined behavior; unpredictable results |
| Use partially-constructed object | Crash, corruption, or silent wrong answer |
| Debug-only check compiled out | Bug exists in production but not in testing |
| Zero masks uninitialized state | Logic errors; valid data treated as invalid |

**Security impact:** Uninitialized memory reads can leak sensitive data or enable exploitation.

**Reliability impact:** Bugs that only manifest under specific memory layouts are nearly impossible to reproduce.

---

## The C++ Solution

### Overview

`EnforcedInit<T>` wraps a value of type `T` and enforces that `init()` is called before `get()`. The check happens at compile time (via deleted default access) or runtime (via assertion/exception). In release builds with `FATP_CONTRACTS_ASSUME`, the check compiles away entirely.

### Fat-P Component

**Component:** `EnforcedInit<T>` (from `EnforcedInit.h`)

**Purpose:** Type-safe wrapper that makes "init before use" a compiler-enforced or runtime-enforced invariant rather than a documentation convention.

### How EnforcedInit Addresses All Four Patterns

| C Pattern | Problem | EnforcedInit Solution |
|-----------|---------|----------------------|
| isInit flag | Manual checking at every use site | Check built into `get()` method |
| State constants / magic numbers | Runtime cost, often debug-only, forgettable | Type-level enforcement, always active |
| Two-phase init | Partial construction visible to callers | Object not accessible until `init()` completes |
| Defensive memset | Masks bugs, runtime cost | Catches bugs at access time instead of hiding them |

### Before and After

**C (manual discipline):**

```c
/* The isInit flag pattern */
struct Config {
    bool isInit;
    int bMemstat;
    int bCoreMutex;
    float maxLoadFactor;
    /* ... 30 more fields ... */
};

/* Every access requires manual checking */
int getMemstat(const struct Config* c) {
    assert(c->isInit);  /* Easy to forget! */
    return c->bMemstat;
}

/* Initialization must set the flag */
void initConfig(struct Config* c) {
    c->bMemstat = DEFAULT_MEMSTAT;
    c->bCoreMutex = 1;
    c->maxLoadFactor = 0.875f;
    /* ... initialize all fields ... */
    c->isInit = true;  /* Easy to forget! */
}
```

**C++ (type-enforced):**

```cpp
#include "EnforcedInit.h"

struct Config {
    fat_p::EnforcedInit<int> bMemstat;
    fat_p::EnforcedInit<int> bCoreMutex;
    fat_p::EnforcedInit<float> maxLoadFactor;
    /* No isInit flag needed */
};

/* Access automatically checked */
int getMemstat(const Config& c) {
    return c.bMemstat.get();  /* Throws/asserts if not initialized */
}

/* Initialization is explicit per-field */
void initConfig(Config& c) {
    c.bMemstat.init(DEFAULT_MEMSTAT);
    c.bCoreMutex.init(1);
    c.maxLoadFactor.init(0.875f);
    /* No flag to remember */
}
```

### What Changes

| Aspect | C Pattern | C++ Solution |
|--------|-----------|--------------|
| Field type | `T` | `EnforcedInit<T>` |
| Read access | `obj.field` | `obj.field.get()` |
| Write access | `obj.field = value` | `obj.field.init(value)` |
| Re-assignment | `obj.field = newValue` | `obj.field.set(newValue)` (after init) |
| Initialization check | Manual `assert(isInit)` | Automatic in `get()` |

### What Stays the Same

- **Memory layout:** `EnforcedInit<T>` stores `T` plus a boolean flag (similar size to manual pattern)
- **Runtime cost in release:** Zero when using `FATP_CONTRACTS_ASSUME`
- **Logic flow:** Same initialization and access patterns, just type-safe
- **Debuggability:** Clear error messages when invariant violated

---

## Migration Mechanics

### Prerequisites

- [ ] C++17 compiler (GCC 7+, Clang 5+, MSVC 2017+)
- [ ] Fat-P `EnforcedInit.h` in include path
- [ ] Existing tests that exercise initialization code paths
- [ ] Build system can compile as C++ (for migrated files)

### Step-by-Step Transformation

**Step 1: Identify isInit flag patterns**

Search for patterns like:

```cpp
// Pattern A: Explicit isInit flag
struct Foo {
    bool isInit;      // or: initialized, ready, valid
    /* fields */
};

// Pattern B: State enum
enum State { UNINIT, READY, RUNNING };
struct Bar {
    State state;
    /* fields */
};

// Pattern C: Magic number validation
#define FOO_MAGIC 0xDEADBEEF
struct Baz {
    uint32_t magic;
    /* fields */
};
```

**Step 2: Add EnforcedInit.h include**

```cpp
#include "EnforcedInit.h"
```

**Step 3: Replace field types**

```cpp
// Before
struct Config {
    bool isInit;
    int memstat;
    int coreMutex;
};

// After
struct Config {
    fat_p::EnforcedInit<int> memstat;
    fat_p::EnforcedInit<int> coreMutex;
    // isInit flag removed — enforcement is per-field
};
```

**Step 4: Update initialization code**

```cpp
// Before
void initConfig(Config& c) {
    c.memstat = 1;
    c.coreMutex = 1;
    c.isInit = true;
}

// After
void initConfig(Config& c) {
    c.memstat.init(1);
    c.coreMutex.init(1);
    // No flag to set
}
```

**Step 5: Update access code**

```cpp
// Before
int getMemstat(const Config& c) {
    assert(c.isInit);
    return c.memstat;
}

// After
int getMemstat(const Config& c) {
    return c.memstat.get();  // Check is automatic
}
```

**Step 6: Update assignment (post-initialization)**

```cpp
// Before
void updateMemstat(Config& c, int newValue) {
    assert(c.isInit);
    c.memstat = newValue;
}

// After
void updateMemstat(Config& c, int newValue) {
    c.memstat.set(newValue);  // Requires prior init()
}
```

### Incremental Migration Strategy

`EnforcedInit<T>` can be adopted one struct at a time:

1. **Start with leaf structs** — those not embedded in other structs
2. **Migrate one field at a time** if needed — mix of raw `T` and `EnforcedInit<T>` is valid
3. **Run tests after each struct** — catch issues early
4. **Remove old isInit flags last** — after all fields migrated

**Coexistence example:**

```cpp
struct Config {
    // Already migrated
    fat_p::EnforcedInit<int> memstat;
    
    // Not yet migrated (still using old pattern)
    bool coreMutexInit;
    int coreMutex;
    
    // Partially migrated structs work fine
};
```

### Common Mistakes

| Mistake | Symptom | Fix |
|---------|---------|-----|
| Forgetting `.get()` on read | Compile error: no conversion from `EnforcedInit<T>` to `T` | Add `.get()` |
| Using `=` instead of `.init()` | Compile error: `operator=` deleted or not available | Use `.init()` for first assignment |
| Calling `.init()` twice | Runtime exception (double initialization) | Call `.init()` exactly once, use `.set()` for updates |
| Forgetting `.init()` call | Runtime exception on first `.get()` | Add initialization at construction site |

---

## Verification

### Compile-Time Guarantees

After migration, these errors become **compile-time failures**:

```cpp
EnforcedInit<int> value;

// ERROR: Cannot implicitly convert to int
int x = value;  // Compile error

// ERROR: No default value access
std::cout << value;  // Compile error (no operator<<)
```

These errors **force** the programmer to explicitly call `.get()`, which includes the initialization check.

### Runtime Validation

| Scenario | Debug Behavior | Release Behavior |
|----------|----------------|------------------|
| `.get()` before `.init()` | Exception or assertion failure | UB (with `ASSUME`) or exception |
| `.init()` called twice | Exception or assertion failure | Exception or assertion failure |
| `.set()` before `.init()` | Exception or assertion failure | UB (with `ASSUME`) or exception |

### Sanitizer Expectations

**AddressSanitizer (ASan):** No direct detection (not a memory error), but may catch downstream corruption.

**UndefinedBehaviorSanitizer (UBSan):** May detect if uninitialized access leads to UB operations.

**Debug builds:** `EnforcedInit<T>` assertions trigger before sanitizers would see the problem.

### Recommended Tests

```cpp
#include "FatPTest.h"
#include "EnforcedInit.h"

namespace fat_p::testing::enforcedinit
{

TEST_CASE(catches_uninitialized_read)
{
    fat_p::EnforcedInit<int> value;
    
    bool threw = false;
    try {
        [[maybe_unused]] int x = value.get();
    } catch (const std::logic_error&) {
        threw = true;
    }
    
    ASSERT_TRUE(threw, "Reading before init() should throw");
    return true;
}

TEST_CASE(catches_double_init)
{
    fat_p::EnforcedInit<int> value;
    value.init(42);
    
    bool threw = false;
    try {
        value.init(43);  // Second init
    } catch (const std::logic_error&) {
        threw = true;
    }
    
    ASSERT_TRUE(threw, "Double init() should throw");
    return true;
}

TEST_CASE(allows_set_after_init)
{
    fat_p::EnforcedInit<int> value;
    value.init(42);
    value.set(43);  // Should succeed
    
    ASSERT_EQ(value.get(), 43, "set() should update value");
    return true;
}

TEST_CASE(migration_regression_isInit_pattern)
{
    // This test verifies that code which previously relied on isInit
    // now correctly uses EnforcedInit
    
    struct Config {
        fat_p::EnforcedInit<int> memstat;
        fat_p::EnforcedInit<int> coreMutex;
    };
    
    Config c;
    
    // Old code might have accessed c.memstat here without checking isInit
    // New code will throw if we try to read before init
    
    c.memstat.init(1);
    c.coreMutex.init(1);
    
    // Now access is valid
    ASSERT_EQ(c.memstat.get(), 1, "Should read initialized value");
    return true;
}

} // namespace
```

---

## Performance Characteristics

### Overhead Measurements

| Build Mode | `init()` + `get()` Overhead | Notes |
|------------|----------------------------|-------|
| Debug (full checks) | ~0.14 ns | Boolean check + branch |
| Release (standard) | ~0.05 ns | Optimized check |
| Release (`FATP_CONTRACTS_ASSUME`) | ~0.0004 ns | Check optimized away entirely |

**Source:** `enforced_init_benchmarks.csv` from Fat-P benchmark suite.

### When Checks Are Elided

With `FATP_CONTRACTS_ASSUME` defined, the compiler assumes all contracts are satisfied. The `get()` method becomes equivalent to direct field access:

```cpp
// With FATP_CONTRACTS_ASSUME, this:
return value.get();

// Compiles to the same code as:
return value.mValue;  // Direct access, no check
```

### Comparison to Manual Pattern

| Approach | Read Cost | Memory Overhead |
|----------|-----------|-----------------|
| No checking | 0 | 0 |
| Manual `assert(isInit)` | ~0.1 ns (debug), 0 (release) | 1 byte per struct |
| `EnforcedInit<T>` (debug) | ~0.14 ns | 1 byte per field |
| `EnforcedInit<T>` (assume) | ~0 ns | 1 byte per field |

The memory overhead is similar (one boolean flag), but `EnforcedInit<T>` tracks per-field rather than per-struct, providing finer granularity.

---

## Summary

| Aspect | C Pattern | C++ with EnforcedInit |
|--------|-----------|----------------------|
| Initialization tracking | Manual `isInit` flag | Per-field type enforcement |
| Check enforcement | Programmer discipline | Compiler + runtime |
| Failure mode | Silent corruption, UB | Immediate exception/assertion |
| Runtime cost (debug) | ~0 ns (often skipped) | ~0.14 ns (always checked) |
| Runtime cost (release) | 0 ns | ~0 ns (with `ASSUME`) |
| Memory overhead | 1 bool per struct | 1 bool per field |
| Granularity | Whole-struct | Per-field |
| Discipline required | High | None |

---

## Where It Loses

- **Memory overhead for many small fields:** Each `EnforcedInit<T>` adds a boolean. For structs with dozens of small fields, this may matter.

- **API verbosity:** `.get()` and `.init()` are more verbose than direct field access. May feel heavyweight for trivial structs with few fields.

- **Not suitable for trivial types in hot loops:** If you're accessing a field millions of times per second in a tight loop, even the debug-mode check may be measurable. Use raw types with careful review, or ensure `FATP_CONTRACTS_ASSUME` is defined.

- **Interop with C code:** `EnforcedInit<T>` is a C++ template. C code cannot use it directly. Boundary code must extract raw values.

- **Aggregate initialization:** Cannot use brace initialization `{value}` — must call `.init()` explicitly.

---

## Read Next

- **User Manual - EnforcedInit** — Full API reference, all methods, policy options
- **Migration Guide - Manual Resource Cleanup to RAII** — Related migration for malloc/free patterns
- **Companion Guide - EnforcedInit** — Design rationale, rejected alternatives
- **Handbook - C++ Design Goals and Migration** — Broader migration strategy

---

*Migration Guide - Initialization Flag Patterns to EnforcedInit v1.0 — January 2025*
