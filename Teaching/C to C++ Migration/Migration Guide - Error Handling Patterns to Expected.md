---
doc_id: MG-EXPECTED-001
doc_type: "Migration Guide"
title: "Error Handling Patterns to Expected"
fatp_components: ["Expected"]
topics: ["C migration", "error handling", "error codes", "errno", "output parameters", "type safety", "monadic operations"]
constraints: ["silent failure propagation", "error code discipline", "out-parameter ownership", "global state mutation"]
cxx_standard: "C++17"
last_verified: "2025-01-08"
audience: ["C developers", "migration teams", "AI assistants"]
status: "draft"
---

# Migration Guide - Error Handling Patterns to Expected

## Scope

This document shows how to migrate three related C error handling patterns to type-safe C++ using `Expected<T, E>`. It uses POSIX APIs and common C idioms as case studies, demonstrating that even well-established conventions cannot prevent error-handling bugs at scale.

## Not Covered

- Full `Expected<T, E>` API reference (see User Manual - Expected)
- Template implementation details (see Companion Guide - Expected)
- Other Fat-P enforcement components (`enforce.h`, `EnforcedInit<T>`)
- General C-to-C++ migration strategy (see Handbook - C++ Design Goals and Migration)
- Async error handling patterns
- Exception vs Expected tradeoffs (see Design Note - Error Strategy)

## Prerequisites

- Familiarity with C error handling idioms (return codes, errno, output parameters)
- Understanding of C++ templates (basic level)
- Awareness of the consequences of unchecked errors
- Access to Fat-P headers

---

## Migration Guide Card

**C Pattern:** Error signaling via integer return codes, errno, and output parameters  
**Why it fails:** Compiler cannot enforce error checking; errors silently propagate  
**C++ Solution:** `Expected<T, E>` — type wrapper that forces acknowledgment of failure  
**Migration effort:** Medium — requires changing return types and call sites  
**Verification method:** Compile-time errors for unchecked returns; runtime exceptions for `.value()` on error  
**Incremental migration:** Yes — can migrate one function at a time; mixed patterns coexist  
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

Three related patterns that C programmers use to signal errors. All share a common weakness: the type system allows silent failure.

### Pattern 1: Integer Error Codes

**Source:** POSIX and virtually every C library

A function returns an integer where 0 (or positive) means success, negative means failure:

```c
/* POSIX-style: 0 = success, -1 = failure (check errno) */
int result = open("/etc/passwd", O_RDONLY);
if (result < 0) {
    perror("open failed");
    return -1;
}

/* Windows-style: 0 = failure, nonzero = success */
HANDLE h = CreateFile(...);
if (h == INVALID_HANDLE_VALUE) {
    /* Handle error */
}
```

The calling convention varies by library: some use 0 for success, others for failure. Some use -1 for error, others use NULL. Some reserve specific negative values for specific errors.

**Why programmers use it:** Simple, efficient, works everywhere.

---

### Pattern 2: errno and Global State

**Source:** [POSIX `errno.h`](https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/errno.h.html)

Error details stored in thread-local global state:

```c
#include <errno.h>
#include <string.h>
#include <stdio.h>

FILE* f = fopen("/nonexistent", "r");
if (f == NULL) {
    /* errno contains the error code */
    int err = errno;  /* MUST copy immediately! */
    fprintf(stderr, "Error %d: %s\n", err, strerror(err));
}
```

The `errno` value is only valid immediately after the failed call:

```c
FILE* f = fopen("/nonexistent", "r");
if (f == NULL) {
    printf("Attempting recovery...\n");  /* May reset errno! */
    int err = errno;  /* WRONG: errno may have changed */
}
```

**Why programmers use it:** Standard mechanism for detailed error information; supported everywhere.

---

### Pattern 3: Output Parameters

**Source:** Common C idiom, OpenSSL, SQLite, Windows API

The actual result is returned through a pointer parameter:

```c
/* OpenSSL-style: returns 1 on success, 0 on failure */
int success = SSL_read(ssl, buffer, sizeof(buffer), &bytes_read);
if (!success) {
    int err = SSL_get_error(ssl, success);
    /* Handle error */
}

/* Windows-style: result through pointer, BOOL return */
DWORD bytes_written;
BOOL ok = WriteFile(handle, data, size, &bytes_written, NULL);
if (!ok) {
    DWORD err = GetLastError();
}
```

The pattern separates "did it work?" from "what's the result?" but creates ownership ambiguity:

```c
/* Who initializes *result? What if the function fails halfway? */
int compute(int input, int* result);

/* Caller doesn't know if result was written on failure */
int value;  /* Uninitialized! */
if (compute(42, &value) != 0) {
    /* Is 'value' valid here? Depends on the function! */
}
```

**Why programmers use it:** Allows returning both status and value; handles non-movable types.

---

### The Common Thread

All three patterns share a fatal weakness: **the compiler cannot enforce error checking**.

| Pattern | Enforcement Mechanism | Weakness |
|---------|----------------------|----------|
| Integer codes | Programmer must check return value | `[[nodiscard]]` helps but can be cast away |
| errno | Programmer must check and copy immediately | Global state; easily clobbered |
| Output parameters | Programmer must provide valid pointer | Ownership unclear; partial writes |

The invariant "errors must be handled" exists only in documentation. The compiler sees a valid integer that can be ignored.

---

## Why They Fail

### The Ignored Return Value

The most common bug: forgetting to check the return value.

```c
/* Bug: return value ignored */
close(fd);  /* What if this fails? */
unlink(path);  /* Silent failure */
fwrite(data, 1, size, file);  /* Partial write ignored */
```

Real-world example from [CWE-252: Unchecked Return Value](https://cwe.mitre.org/data/definitions/252.html):

```c
/* Actual vulnerability pattern */
char* buf = malloc(size);
read(fd, buf, size);  /* Bug: read() return value ignored */
process(buf);  /* May process uninitialized memory */
```

**Impact:** Data corruption, security vulnerabilities, silent failures.

### The errno Race

The errno value must be captured immediately, but intervening code can clobber it:

```c
FILE* f = fopen(path, "r");
if (f == NULL) {
    log_error("Failed to open %s", path);  /* log_error may call fprintf, etc. */
    
    /* Bug: errno may have been overwritten by log_error internals */
    if (errno == ENOENT) {  /* WRONG: errno may not reflect fopen failure */
        create_file(path);
    }
}
```

This is especially insidious in multi-threaded code where errno is thread-local but the pattern feels racy.

### The Output Parameter Ambiguity

When a function fails, the state of output parameters is often unspecified:

```c
struct Result {
    int value;
    char* message;  /* Who owns this? */
};

int compute(int input, struct Result* out) {
    out->message = malloc(100);  /* Allocated before knowing if we'll succeed */
    if (input < 0) {
        return -1;  /* Bug: leaked out->message */
    }
    out->value = input * 2;
    sprintf(out->message, "Computed: %d", out->value);
    return 0;
}
```

The caller cannot know:
1. Whether `out` was modified at all
2. Whether partial modifications need cleanup
3. Who owns dynamically allocated fields

---

## The C++ Solution

`Expected<T, E>` encodes success-or-failure in the type system. The compiler enforces that you cannot access the value without acknowledging the possibility of error.

### Basic Usage

```cpp
#include "Expected.h"

// Function that can fail returns Expected
fat_p::Expected<int, std::string> parse_int(const std::string& s) {
    try {
        return std::stoi(s);  // Implicit conversion to success
    } catch (const std::exception& e) {
        return fat_p::unexpected(std::string("Parse failed: ") + e.what());
    }
}

// Caller MUST handle both cases
void use_value() {
    auto result = parse_int("42");
    
    // Option 1: Check explicitly
    if (result.has_value()) {
        std::cout << "Got: " << result.value() << "\n";
    } else {
        std::cerr << "Error: " << result.error() << "\n";
    }
    
    // Option 2: Use value_or for default
    int val = result.value_or(-1);
    
    // Option 3: Use monadic operations
    auto doubled = result.map([](int x) { return x * 2; });
}
```

### Key Properties

| Property | Behavior |
|----------|----------|
| Default state | Contains default-constructed `T` (if possible) |
| `.value()` on error | Throws `bad_expected_access<E>` |
| `.error()` on success | Undefined behavior (asserts in debug) |
| Assignment | Replaces current state; destroys old value/error |
| Comparison | Compares states first, then values/errors |

### The Type Enforces Handling

```cpp
fat_p::Expected<int, std::string> compute();

void bad_code() {
    compute();  // [[nodiscard]] warning: ignoring return value
    
    auto result = compute();
    int x = result;  // ERROR: no implicit conversion to int
    
    std::cout << result.value();  // May throw if error state
}

void good_code() {
    auto result = compute();
    
    if (result) {  // Explicit check (operator bool)
        std::cout << *result;  // operator* for value access
    } else {
        std::cerr << result.error();
    }
}
```

---

## Migration Mechanics

### Step-by-Step: Integer Error Codes

**Step 1: Identify the pattern**

```c
// Before: returns 0 on success, -1 on failure, sets errno
int read_config(const char* path, Config* out);
```

**Step 2: Define the error type**

```cpp
// Choose an appropriate error type
enum class ConfigError {
    FileNotFound,
    ParseError,
    PermissionDenied
};

// Or use std::string for descriptive errors
// Or use std::error_code for system errors
```

**Step 3: Change the signature**

```cpp
// After: returns value on success, error on failure
fat_p::Expected<Config, ConfigError> read_config(const std::string& path);
```

**Step 4: Update the implementation**

```cpp
// Before
int read_config(const char* path, Config* out) {
    FILE* f = fopen(path, "r");
    if (!f) {
        return -1;
    }
    // ... parse into *out ...
    fclose(f);
    return 0;
}

// After
fat_p::Expected<Config, ConfigError> read_config(const std::string& path) {
    FILE* f = fopen(path.c_str(), "r");
    if (!f) {
        return fat_p::unexpected(ConfigError::FileNotFound);
    }
    
    Config config;
    // ... parse into config ...
    
    fclose(f);
    return config;  // Implicit conversion to Expected
}
```

**Step 5: Update call sites**

```cpp
// Before
Config cfg;
if (read_config("/etc/app.conf", &cfg) != 0) {
    perror("read_config");
    return EXIT_FAILURE;
}
use_config(cfg);

// After
auto result = read_config("/etc/app.conf");
if (!result) {
    std::cerr << "Config error: " << static_cast<int>(result.error()) << "\n";
    return EXIT_FAILURE;
}
use_config(*result);

// Or with monadic style
read_config("/etc/app.conf")
    .map([](const Config& c) { use_config(c); })
    .or_else([](ConfigError e) { 
        std::cerr << "Error: " << static_cast<int>(e) << "\n"; 
    });
```

### Step-by-Step: errno Pattern

**Step 1: Identify errno usage**

```c
int fd = open(path, O_RDONLY);
if (fd < 0) {
    int err = errno;
    // Handle based on err
}
```

**Step 2: Use std::error_code or custom error**

```cpp
#include <system_error>

fat_p::Expected<int, std::error_code> safe_open(const std::string& path) {
    int fd = open(path.c_str(), O_RDONLY);
    if (fd < 0) {
        return fat_p::unexpected(
            std::error_code(errno, std::system_category())
        );
    }
    return fd;
}
```

**Step 3: errno is captured at the failure site**

```cpp
// The error code is captured immediately and travels with the result
auto result = safe_open("/etc/passwd");
// ... arbitrary code here; errno may change ...
if (!result) {
    // result.error() still contains the original errno value
    std::cerr << result.error().message() << "\n";
}
```

### Step-by-Step: Output Parameters

**Step 1: Identify the pattern**

```c
// Result through pointer, status through return
int parse_json(const char* input, JsonValue* out, char** error_msg);
```

**Step 2: Return the value directly**

```cpp
fat_p::Expected<JsonValue, std::string> parse_json(std::string_view input);
```

**Step 3: No more ownership ambiguity**

```cpp
// Before: Who owns error_msg? What if parse fails halfway?
JsonValue val;
char* err = NULL;
if (parse_json(input, &val, &err) != 0) {
    printf("Error: %s\n", err);
    free(err);  // Caller must remember to free
    return;
}
// Is val fully initialized? Depends on implementation.

// After: Ownership is clear through value semantics
auto result = parse_json(input);
if (!result) {
    std::cerr << "Error: " << result.error() << "\n";
    return;  // No cleanup needed
}
// result.value() is guaranteed fully constructed
use_value(*result);
```

### Using EXPECTED_TRY for Error Propagation

The `EXPECTED_TRY` macro provides Rust-like `?` operator semantics:

```cpp
fat_p::Expected<ProcessedData, std::string> process_file(const std::string& path) {
    // EXPECTED_TRY extracts value or returns error early
    EXPECTED_TRY(content, read_file(path));
    EXPECTED_TRY(parsed, parse_content(content));
    EXPECTED_TRY(validated, validate(parsed));
    
    return transform(validated);  // Only reached if all succeeded
}

// Equivalent to:
fat_p::Expected<ProcessedData, std::string> process_file_verbose(const std::string& path) {
    auto content_result = read_file(path);
    if (!content_result) {
        return fat_p::unexpected(std::move(content_result).error());
    }
    auto content = std::move(content_result).value();
    
    // ... repeat for each step ...
}
```

### Incremental Migration Strategy

`Expected<T, E>` can be adopted one function at a time:

1. **Start with leaf functions** — those that don't call other error-returning functions
2. **Create wrapper functions** if needed — wrap C functions that use errno
3. **Migrate call chains bottom-up** — so inner functions return Expected before outer ones
4. **Use `.value()` at boundaries** — when interfacing with code not yet migrated

**Coexistence example:**

```cpp
// Old C-style function (not yet migrated)
int legacy_compute(int input, int* output);

// New wrapper returning Expected
fat_p::Expected<int, std::string> compute(int input) {
    int result;
    if (legacy_compute(input, &result) != 0) {
        return fat_p::unexpected("legacy_compute failed");
    }
    return result;
}

// New code uses Expected throughout
fat_p::Expected<int, std::string> process(int x) {
    EXPECTED_TRY(computed, compute(x));
    return computed * 2;
}

// At boundary with old code that expects int
void old_caller() {
    auto result = process(42);
    // Extract value for old code (throws on error)
    int value = result.value();
    old_function(value);
}
```

### Common Mistakes

| Mistake | Symptom | Fix |
|---------|---------|-----|
| Ignoring the return value | Compiler warning (if `[[nodiscard]]`) | Always bind to a variable and check |
| Calling `.value()` without checking | `bad_expected_access` exception thrown | Check `.has_value()` first or use `.value_or()` |
| Calling `.error()` on success | Undefined behavior (assertion in debug) | Check `!has_value()` first |
| Mixing error types in a call chain | Compile error: incompatible Expected types | Use `.transform_error()` to convert |
| Forgetting to propagate errors | Logic error: success returned on failure | Use `EXPECTED_TRY` macro |

---

## Verification

### Compile-Time Guarantees

After migration, these patterns become **compile-time failures**:

```cpp
fat_p::Expected<int, std::string> compute();

// ERROR: [[nodiscard]] - return value ignored
compute();

// ERROR: No implicit conversion to int
int x = compute();

// ERROR: No operator<< for Expected
std::cout << compute();
```

These errors **force** the programmer to explicitly handle the Expected, preventing accidental error ignorance.

### Runtime Validation

| Scenario | Debug Behavior | Release Behavior |
|----------|----------------|------------------|
| `.value()` on error | Throws `bad_expected_access<E>` | Throws `bad_expected_access<E>` |
| `.error()` on success | Assertion failure | UB |
| `*exp` on error | Assertion failure (in debug) | UB |
| `EXPECTED_TRY` on error | Early return with error | Early return with error |

### Recommended Tests

```cpp
#include "FatPTest.h"
#include "Expected.h"

namespace fat_p::testing::expected
{

TEST_CASE(value_access_on_error_throws)
{
    Expected<int, std::string> err = unexpected(std::string("failed"));
    
    bool threw = false;
    try {
        [[maybe_unused]] int x = err.value();
    } catch (const bad_expected_access<std::string>& e) {
        threw = true;
        ASSERT_EQ(e.error(), "failed", "Exception should contain error");
    }
    
    ASSERT_TRUE(threw, "Accessing value on error should throw");
    return true;
}

TEST_CASE(expected_try_propagates_error)
{
    auto inner = []() -> Expected<int, std::string> {
        return unexpected(std::string("inner error"));
    };
    
    auto outer = [&]() -> Expected<int, std::string> {
        EXPECTED_TRY(val, inner());
        return val * 2;  // Should not reach
    };
    
    auto result = outer();
    ASSERT_FALSE(result.has_value(), "Error should propagate");
    ASSERT_EQ(result.error(), "inner error", "Original error preserved");
    return true;
}

TEST_CASE(monadic_map_on_error_preserves_error)
{
    Expected<int, std::string> err = unexpected(std::string("failed"));
    
    auto mapped = err.map([](int x) { return x * 2; });
    
    ASSERT_FALSE(mapped.has_value(), "map should preserve error state");
    ASSERT_EQ(mapped.error(), "failed", "Error should be unchanged");
    return true;
}

TEST_CASE(migration_regression_errno_capture)
{
    // Simulate errno-based function
    auto safe_operation = []() -> Expected<int, std::error_code> {
        errno = ENOENT;  // Simulate failure
        return unexpected(std::error_code(errno, std::system_category()));
    };
    
    auto result = safe_operation();
    
    // Do other things that might change errno
    errno = 0;
    fopen("/dev/null", "r");  // Might set errno
    
    // Original error is preserved
    ASSERT_FALSE(result.has_value(), "Should be error");
    ASSERT_EQ(result.error().value(), ENOENT, "Original errno preserved");
    return true;
}

} // namespace
```

---

## Performance Characteristics

### Overhead Measurements

| Build Mode | Expected<int, int> | Error Code Pattern | Notes |
|------------|-------------------|-------------------|-------|
| Release (TrivialStorage) | 0 ns | 0 ns | Register-passed, identical to int pair |
| Release (UnionStorage) | ~0.3 ns | 0 ns | Minimal overhead from discriminator |
| Debug (full checks) | ~1.2 ns | 0 ns | Includes assertion checks |

**Source:** Fat-P benchmark suite internal measurements.

### Memory Layout

```cpp
// Expected<int, int> with TrivialStorage: 8 bytes (2 ints, no padding)
// Equivalent to: struct { int value; int error; bool has_value; } padded

// Expected<std::string, std::string> with UnionStorage:
// Same size as max(sizeof(string), sizeof(string)) + 1 byte discriminator + padding
```

### When to Use TrivialStorage

For HPC scenarios with trivially copyable types, use `TrivialStorage` for zero-overhead:

```cpp
// Zero-overhead for hot paths with trivial types
using FastExpected = fat_p::Expected<int, int, fat_p::TrivialStorage>;

FastExpected hot_path_compute(int x) {
    if (x < 0) return fat_p::unexpected(EINVAL);
    return x * 2;
}
```

### Comparison to Manual Pattern

| Approach | Per-Call Cost | Memory | Error Propagation |
|----------|--------------|--------|-------------------|
| Integer return code | 0 ns | 4 bytes | Manual checking |
| errno | 0 ns + TLS lookup | 4 bytes global | Manual, fragile |
| Output parameter | 0 ns | Varies | Manual checking |
| Expected (trivial) | 0 ns | 8+ bytes | Type-enforced |
| Expected (union) | ~0.3 ns | sizeof(T∪E)+1 | Type-enforced |

The overhead of Expected is negligible compared to the safety guarantees.

---

## Summary

| Aspect | C Pattern | C++ with Expected |
|--------|-----------|-------------------|
| Error signaling | Integer codes, errno, out-params | Typed Either value |
| Check enforcement | Programmer discipline | Compiler + runtime |
| Failure mode | Silent ignorance, UB | Compile error or exception |
| Error preservation | errno clobbered easily | Travels with result |
| Ownership | Ambiguous for out-params | Clear value semantics |
| Composition | Manual if-chains | Monadic operations |
| Runtime cost | 0 | ~0 (trivial types) |

---

## Where It Loses

- **ABI boundaries:** Expected is a template; cannot be passed across C ABI boundaries. Use conversion functions at FFI boundaries.

- **Hot loops with trivial errors:** Even with `TrivialStorage`, the discriminator check exists. For extreme hot paths where error is impossible, raw values may be appropriate.

- **Code verbosity:** `EXPECTED_TRY` helps, but error handling code is more visible than ignored return values. This is arguably a feature.

- **Exception interop:** Code that catches exceptions must convert to Expected at boundaries. Use try-catch wrappers.

- **Legacy code integration:** Wrapping every C function in Expected adds boilerplate. Consider wrapping only at module boundaries.

- **Error type consistency:** Different libraries may use different error types. Use `transform_error()` to adapt.

---

## Read Next

- **User Manual - Expected** — Full API reference, all methods, storage policies
- **Migration Guide - Initialization Patterns to EnforcedInit** — Related migration for uninitialized state
- **Migration Guide - Manual Resource Cleanup to ScopeGuard** — RAII for resource management
- **Companion Guide - Expected** — Design rationale, rejected alternatives
- **Handbook - C++ Error Handling Strategy** — When to use Expected vs exceptions

---

*Migration Guide - Error Handling Patterns to Expected v1.0 — January 2025*
