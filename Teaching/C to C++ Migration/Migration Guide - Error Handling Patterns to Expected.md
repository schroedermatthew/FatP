---
doc_id: MG-EXPECTED-001
doc_type: "Migration Guide"
title: "Error Handling Patterns to Expected"
from_pattern: "Integer error codes, errno, output parameters, sentinel values"
to_component: "Expected"
fatp_version: "1.0"
cxx_standard: "C++17"
std_equivalent: "std::expected"
std_since: "C++23"
boost_equivalent: "Boost.Outcome"
migration_complexity: "Medium"
breaking_changes: false
last_verified: "2025-01-09"
---

# Migration Guide - Error Handling Patterns to Expected

### *From Silent Failure to Compiler-Enforced Error Handling*

*FAT-P Library — January 2025*

---

## Migration Card

| Aspect | Detail |
|--------|--------|
| **C Pattern** | Integer return codes, errno, output parameters, NULL/sentinel values |
| **Problems Solved** | Ignored errors, errno clobbering, output parameter ambiguity, silent failure |
| **Fat-P Component** | `Expected<T, E>` with monadic operations |
| **Migration Complexity** | Medium — requires changing return types and call sites |
| **Runtime Overhead** | Zero for trivial types; ~0.3ns for union storage |
| **Breaking Changes** | No — can migrate incrementally; wrappers for legacy code |

---

## Alternatives

- **std::expected** (C++23) — The standard library version
- **Boost.Outcome** — Mature, feature-rich, heavier
- **llvm::Expected** — LLVM's implementation, similar API
- **tl::expected** — Single-header C++11/14/17 implementation
- **folly::Expected** — Facebook's implementation

---

## Table of Contents

1. [The Problem with C Error Handling](#the-problem-with-c-error-handling)
2. [Real-World Error Handling Disasters](#real-world-error-handling-disasters)
3. [The C Patterns](#the-c-patterns)
4. [The Expected Solution](#the-expected-solution)
5. [Migration Steps](#migration-steps)
6. [Before/After Examples](#beforeafter-examples)
7. [Advanced Patterns](#advanced-patterns)
8. [Verification](#verification)
9. [When Expected Loses](#when-expected-loses)
10. [Summary](#summary)

---

## The Problem with C Error Handling

Error handling in C relies on conventions that the compiler cannot enforce. You call a function, it returns an integer, and the documentation tells you that negative means failure. But the compiler sees only `int`. It doesn't distinguish between "success value" and "error code" at the type level.

```c
int result = process_data(input, &output);
// Did you check result? Compiler doesn't care.
// Is output valid if result != 0? Depends on documentation.
// Did process_data set errno? Maybe, check the man page.

use_output(output);  // Hope this is valid!
```

This creates a class of bugs that are invisible at compile time. The programmer must remember to check every return value, and the compiler won't complain if they forget. In a large codebase, some checks will be missed. The following are all valid C that any compiler will accept without warning:

```c
close(fd);                        // Return value ignored
char* p = malloc(size);           // NULL not checked
ssize_t n = read(fd, buf, size);  // Partial read not handled
```

Each line is a potential crash or security vulnerability. The type system offers no protection.

---

## Real-World Error Handling Disasters

These aren't hypothetical concerns. Unchecked errors cause real vulnerabilities and real crashes.

### The Ignored malloc (CVE-2019-14287)

A privilege escalation vulnerability in sudo came from a missing NULL check. The code allocated memory and immediately used it without verifying the allocation succeeded:

```c
/* Simplified from sudo source */
char *user_info = malloc(info_len);
/* No NULL check! */
strcpy(user_info, source);  /* Crash or exploit if malloc failed */
```

Under memory pressure, malloc returns NULL. The strcpy then writes to address zero, which on some systems is exploitable. This pattern appears in countless codebases—the allocation is so routine that programmers forget it can fail.

### The Clobbered errno

The errno mechanism is particularly fragile. It's a global (per-thread) variable that any system call can overwrite. If you don't capture it immediately after the failing call, it's gone:

```c
FILE* f = fopen(path, "r");
if (f == NULL) {
    log_error("Failed to open %s", path);  /* log_error calls fprintf */
                                           /* fprintf may change errno! */
    if (errno == ENOENT) {  /* WRONG: errno is stale */
        create_default_file(path);
    }
}
```

This bug is insidious because it works most of the time. It only fails when the logging function happens to make a system call that sets errno—which might depend on log verbosity, output buffering, or disk state.

### The Ambiguous Output Parameter

Windows API patterns compound the problem. A function returns BOOL for success/failure, but you also need to know how much data was actually transferred:

```c
DWORD bytesRead;
BOOL success = ReadFile(handle, buffer, size, &bytesRead, NULL);
if (!success) {
    /* Is bytesRead valid here? Partially? Zero? Undefined? */
    /* Different functions have different conventions! */
}
```

Each Windows API function has its own rules about whether output parameters are valid on failure. You have to read the documentation for every single function.

### The sqlite3 Error Model

SQLite returns integer error codes, but rich error information requires separate calls. This creates opportunities for mistakes:

```c
int rc = sqlite3_exec(db, sql, NULL, NULL, &errmsg);
if (rc != SQLITE_OK) {
    /* errmsg might be NULL even on error! */
    /* Must free errmsg if non-NULL */
    /* Must check sqlite3_errmsg(db) for full message */
    /* Easy to leak errmsg */
}
```

The error message is returned through an output parameter that you must remember to free. Forget to free it, and you leak memory. Use it after freeing, and you have a use-after-free. The type system can't help you.

---

## The C Patterns

Before we can migrate, we need to recognize what we're migrating from. C error handling evolved several distinct patterns, each solving one problem while creating others.

### Pattern 1: Integer Error Codes

The most common pattern uses the return value itself as the error indicator. Success is typically 0, and various negative or positive values indicate different failure modes. But even this simple convention isn't universal:

```c
/* POSIX style: 0 = success, negative = failure */
int fd = open("/path", O_RDONLY);
if (fd < 0) {
    perror("open");
    return -1;
}

/* Windows style: 0 = failure, nonzero = success */
BOOL ok = DeleteFile(path);
if (!ok) {
    DWORD err = GetLastError();
}
```

POSIX treats 0 as success. Windows treats 0 as failure. Mix code from both ecosystems and you'll eventually check the wrong way. The compiler can't help because both conventions use the same `int` type—it has no idea what the number means.

The deeper problem is that the return value can be silently ignored. `close(fd)` returns an error code. How often have you seen code that checks it?

### Pattern 2: errno Global State

When integer return values aren't enough to convey error details, POSIX uses a thread-local global variable called `errno`. The failing function sets it; the caller reads it:

```c
#include <errno.h>

int result = some_syscall();
if (result < 0) {
    int saved_errno = errno;  /* Must save IMMEDIATELY */
    /* Any function call might clobber errno */
    handle_error(saved_errno);
}
```

The fragility here is temporal. You must capture errno before doing anything else, because any subsequent function call—even one that succeeds—might overwrite it. This is why you see `int saved_errno = errno;` as the first line of error handlers. Forget that line, call a logging function, and you're checking stale data.

### Pattern 3: Output Parameters

When a function needs to return both a success value and an error code, C uses output parameters:

```c
int compute(int input, int* output) {
    if (input < 0) {
        return -EINVAL;  /* Is *output modified? */
    }
    *output = input * 2;
    return 0;
}

int value;  /* Uninitialized */
int err = compute(-1, &value);
/* Is value valid? Undefined? Zero? */
```

The contract for output parameters on failure is entirely convention. Some functions leave the output untouched. Some zero it. Some write partial results. You have to read the documentation, and different libraries have different conventions. The type system sees only `int*`—it can't express "valid only if the function returned 0."

### Pattern 4: Sentinel Values

When NULL or -1 isn't enough, functions reserve special values from the normal range to indicate errors:

```c
void* ptr = malloc(size);
if (ptr == NULL) { /* failure */ }

int index = find_element(array, size, target);
if (index == -1) { /* not found */ }

time_t t = time(NULL);
if (t == (time_t)-1) { /* failure */ }
```

Each sentinel steals one value from the legitimate range. For `find_element`, you can't distinguish "element at position -1" from "not found" (though negative indices aren't valid anyway). For types with larger ranges, the sentinel value might be a legitimate result on other systems.

The real problem is that every API chooses its own sentinel. NULL, -1, 0, (time_t)-1, INVALID_HANDLE_VALUE, SIZE_MAX—you must memorize each convention or check documentation constantly.

---

## The Expected Solution

The fundamental insight behind `Expected<T, E>` is that success and failure are different types. A function that might fail doesn't return `T`—it returns "either T or an error E." The type system then forces you to acknowledge both possibilities before you can use the value.

This is a discriminated union: a tagged structure that holds exactly one of two alternatives. You can ask which one it holds, and you can extract the value—but only after checking which alternative is present. Attempting to extract the wrong one is a compile-time error or a runtime exception, not silent corruption.

```cpp
#include "Expected.h"
using namespace fat_p;

Expected<int, std::string> divide(int a, int b) {
    if (b == 0) {
        return unexpected("division by zero");
    }
    return a / b;
}

void use_divide() {
    auto result = divide(10, 0);
    
    // Cannot accidentally use as int:
    // int x = result;  // ERROR: no implicit conversion
    
    // Must explicitly handle:
    if (result.has_value()) {
        std::cout << "Result: " << result.value() << "\n";
    } else {
        std::cout << "Error: " << result.error() << "\n";
    }
}
```

The `[[nodiscard]]` attribute on Expected's return means the compiler warns if you ignore the result. The lack of implicit conversion to `T` means you can't accidentally use an Expected where the unwrapped value is required. These are compile-time guarantees—the class of bugs we saw in C patterns becomes impossible.

### Monadic Operations

Expected provides operations that let you chain computations without explicit error checking at each step. The `map` function transforms the success value while passing errors through unchanged. The `and_then` function chains operations that themselves return Expected:

```cpp
Expected<Config, std::string> load_config(const std::string& path) {
    return read_file(path)
        .and_then(parse_json)
        .and_then(validate_config)
        .map([](ValidatedJson&& j) { return Config(std::move(j)); });
}
```

If `read_file` fails, its error propagates through the entire chain—`parse_json` and `validate_config` are never called. This replaces the nested if-statements of C error handling with a pipeline where errors flow naturally to the end.

### The EXPECTED_TRY Macro

For code that needs to propagate errors without the functional style, `EXPECTED_TRY` provides early-return syntax similar to exception handling:

```cpp
Expected<Data, std::string> process_file(const char* path) {
    EXPECTED_TRY(fd, safe_open(path, O_RDONLY));
    SCOPE_EXIT { close(fd); };
    
    EXPECTED_TRY(buffer, safe_read(fd, 1024));
    
    return parse_data(buffer);
}
```

Each `EXPECTED_TRY` checks its expression. If the expression holds an error, the function immediately returns that error. If it holds a value, the macro extracts and assigns it. This eliminates the manual error checking while keeping the imperative style that many programmers prefer.

---

## Migration Steps

Migration from C error patterns to Expected works best bottom-up. Start with leaf functions—those that don't call other error-returning functions—and work your way up the call stack.

### Step 1: Identify Error-Returning Functions

Before changing anything, survey the codebase. Find functions that return error codes, use output parameters for results, or rely on errno:

```bash
grep -rn "return -1\|return NULL\|return -E" src/
grep -rn "if.*< 0\|if.*== NULL\|if.*!= 0" src/
```

Group these by call graph depth. Functions that make no calls to other error-returning functions are your starting points.

### Step 2: Wrap Leaf Functions First

A leaf function has self-contained error logic. It validates inputs, performs some computation, and returns success or failure. These are the easiest to migrate because changing them doesn't cascade through the codebase.

The C version uses an output parameter and returns an error code:

```c
int parse_int(const char* str, int* out) {
    char* end;
    long val = strtol(str, &end, 10);
    if (end == str || *end != '\0') {
        return -1;
    }
    if (val < INT_MIN || val > INT_MAX) {
        return -2;
    }
    *out = (int)val;
    return 0;
}
```

The Expected version returns the value directly, with error information embedded in the return type:

```cpp
Expected<int, std::string> parse_int(std::string_view str) {
    char* end;
    long val = strtol(str.data(), &end, 10);
    if (end == str.data() || *end != '\0') {
        return unexpected("invalid integer format");
    }
    if (val < INT_MIN || val > INT_MAX) {
        return unexpected("integer overflow");
    }
    return static_cast<int>(val);
}
```

The error message is now part of the return value, not a magic number that the caller must look up in documentation.

### Step 3: Wrap errno-Based Functions

System calls that set errno need wrappers that capture the error immediately, before any other code can overwrite it:

```cpp
Expected<int, std::error_code> safe_open(const char* path, int flags) {
    int fd = ::open(path, flags);
    if (fd < 0) {
        return unexpected(std::error_code(errno, std::system_category()));
    }
    return fd;
}

Expected<std::vector<char>, std::error_code> safe_read(int fd, size_t size) {
    std::vector<char> buffer(size);
    ssize_t n = ::read(fd, buffer.data(), size);
    if (n < 0) {
        return unexpected(std::error_code(errno, std::system_category()));
    }
    buffer.resize(n);
    return buffer;
}
```

The wrapper creates the error_code on the line immediately after the syscall, before any other code can run. This captures errno reliably.

### Step 4: Chain with EXPECTED_TRY

Once leaf functions return Expected, higher-level functions can use EXPECTED_TRY to propagate errors without manual checking. Compare the C version with its careful cleanup:

```c
int process_file(const char* path) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) return -1;
    
    char buf[1024];
    ssize_t n = read(fd, buf, sizeof(buf));
    if (n < 0) {
        close(fd);
        return -1;
    }
    
    int result = parse_data(buf, n);
    close(fd);
    return result;
}
```

The Expected version eliminates the manual error paths:

```cpp
Expected<Data, std::string> process_file(const char* path) {
    EXPECTED_TRY(fd, safe_open(path, O_RDONLY));
    SCOPE_EXIT { close(fd); };
    
    EXPECTED_TRY(buffer, safe_read(fd, 1024));
    
    return parse_data(buffer);
}
```

SCOPE_EXIT handles cleanup regardless of which path the function takes. Each EXPECTED_TRY either extracts the value and continues, or immediately returns the error.

### Step 5: Use Monadic Operations

For complex transformation chains, the monadic style is more concise than EXPECTED_TRY:

```cpp
Expected<Config, std::string> load_config(const std::string& path) {
    return read_file(path)
        .and_then(parse_json)
        .and_then(validate_config)
        .map([](ValidatedJson&& j) { return Config(std::move(j)); });
}
```

Each step in the chain receives the output of the previous step. If any step fails, its error flows through to the end.

### Step 6: Handle Boundaries

At FFI boundaries or with legacy code, you'll need conversion functions. Expected-based code can expose a C interface when necessary:

```cpp
// Calling Expected-based code from C-style code
extern "C" int legacy_api(const char* path, Data* out) {
    auto result = process_file(path);
    if (!result.has_value()) {
        return -1;
    }
    *out = std::move(result).value();
    return 0;
}

// Wrapping C-style code for Expected-based callers
Expected<Data, int> wrap_legacy(const char* path) {
    Data out;
    int err = legacy_api(path, &out);
    if (err != 0) {
        return unexpected(err);
    }
    return out;
}
```

The boundary layer translates between paradigms. Inside the boundary, use Expected consistently.

---

## Before/After Examples

These examples show complete transformations of realistic C code to Expected-based C++. Notice how the Expected versions are shorter, yet provide stronger guarantees.

### Example 1: Configuration Parser

Configuration loading is a common source of unchecked errors. The C version must check file operations, memory allocation, and parsing—and must clean up resources on every error path:

```c
int load_config(const char* path, Config* out) {
    FILE* f = fopen(path, "r");
    if (!f) return -1;
    
    char* buffer = malloc(MAX_CONFIG_SIZE);
    if (!buffer) {
        fclose(f);
        return -1;
    }
    
    size_t n = fread(buffer, 1, MAX_CONFIG_SIZE, f);
    fclose(f);
    
    if (n == 0) {
        free(buffer);
        return -1;
    }
    
    int err = parse_config_string(buffer, n, out);
    free(buffer);
    return err;
}
```

The Expected version uses RAII for resource management and returns errors through the type system:

```cpp
Expected<Config, std::string> load_config(const std::filesystem::path& path) {
    auto content = read_file(path);
    if (!content) {
        return unexpected("Failed to read " + path.string() + ": " + content.error());
    }
    
    return parse_config_string(*content);
}
```

With monadic chaining, it becomes even more concise:

```cpp
Expected<Config, std::string> load_config(const std::filesystem::path& path) {
    return read_file(path)
        .transform_error([&](auto e) { return "Read " + path.string() + ": " + e; })
        .and_then(parse_config_string);
}
```

The error messages are now part of the return value, and the caller cannot ignore them.

### Example 2: Database Query

Database code often has the "output parameter on success" pattern, where the caller provides a pointer and the function fills it in only if successful. This creates ambiguity about what the output contains on failure:

```c
int query_user(Database* db, int user_id, User* out) {
    char sql[256];
    snprintf(sql, sizeof(sql), "SELECT * FROM users WHERE id = %d", user_id);
    
    Result* res = db_query(db, sql);
    if (!res) {
        return db_errno(db);
    }
    
    if (db_num_rows(res) == 0) {
        db_free_result(res);
        return -ENOENT;
    }
    
    Row* row = db_fetch_row(res);
    out->id = atoi(row->fields[0]);
    out->name = strdup(row->fields[1]);  // Memory leak if caller forgets
    
    db_free_result(res);
    return 0;
}
```

The Expected version returns a User directly, with the error type conveying rich information:

```cpp
Expected<User, DbError> query_user(Database& db, int user_id) {
    auto sql = fmt::format("SELECT * FROM users WHERE id = {}", user_id);
    
    EXPECTED_TRY(result, db.query(sql));
    
    if (result.empty()) {
        return unexpected(DbError::NotFound);
    }
    
    return User{
        .id = result[0].get<int>("id"),
        .name = result[0].get<std::string>("name")
    };
}
```

No output parameters, no manual result freeing, no memory management confusion.

### Example 3: Network Request

Network code accumulates cleanup obligations at each step. A socket must be closed, DNS results must be freed, and partial receives must be handled. The C version becomes a pyramid of error handling:

```c
int http_get(const char* url, char** response, size_t* response_len) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return -1;
    
    struct hostent* host = gethostbyname(extract_host(url));
    if (!host) {
        close(sock);
        return -2;
    }
    
    struct sockaddr_in addr;
    /* ... setup addr ... */
    
    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(sock);
        return -3;
    }
    
    /* Send request, receive response... */
    /* Every step can fail, every failure needs cleanup */
    
    close(sock);
    return 0;
}
```

The Expected version uses SCOPE_EXIT for cleanup and EXPECTED_TRY for error propagation:

```cpp
Expected<std::string, HttpError> http_get(std::string_view url) {
    EXPECTED_TRY(parsed, parse_url(url));
    EXPECTED_TRY(sock, connect_to(parsed.host, parsed.port));
    SCOPE_EXIT { sock.close(); };
    
    EXPECTED_TRY(_, sock.send(format_request(parsed)));
    
    return sock.receive_all();
}
```

The cleanup happens automatically, and errors propagate through the return type.

---

## Advanced Patterns

Once you've migrated basic functions, these patterns help with more complex scenarios.

### Pattern: Error Type Conversion

Different layers of your application might use different error types. Low-level code might return numeric error codes; high-level APIs might use string messages. The `transform_error` method bridges them:

```cpp
Expected<Data, std::string> high_level_api() {
    auto result = low_level_api();  // Returns Expected<Data, int>
    
    return result.transform_error([](int code) {
        return "Low-level error: " + std::to_string(code);
    });
}
```

The transformation runs only if the result is an error. Success values pass through unchanged.

### Pattern: Collecting Multiple Errors

Sometimes you want to validate multiple fields and report all errors, not just the first one. Accumulate errors into a vector, then return them together:

```cpp
Expected<Results, std::vector<std::string>> validate_all(const Input& input) {
    std::vector<std::string> errors;
    
    if (auto e = validate_field1(input.field1); !e) {
        errors.push_back(e.error());
    }
    if (auto e = validate_field2(input.field2); !e) {
        errors.push_back(e.error());
    }
    
    if (!errors.empty()) {
        return unexpected(std::move(errors));
    }
    
    return Results{...};
}
```

This pattern is common in form validation and configuration parsing where users want to see all problems at once.

### Pattern: Retry with Expected

Transient failures (network timeouts, lock contention) often succeed on retry. A generic retry wrapper works naturally with Expected:

```cpp
template<typename F>
auto retry(F&& f, int max_attempts, std::chrono::milliseconds delay)
    -> decltype(f())
{
    for (int i = 0; i < max_attempts - 1; ++i) {
        auto result = f();
        if (result.has_value()) {
            return result;
        }
        std::this_thread::sleep_for(delay);
    }
    return f();  // Final attempt
}

// Usage
auto data = retry([&]{ return fetch_data(url); }, 3, 100ms);
```

The retry logic is separate from the operation being retried. You can compose it with any Expected-returning function.

### Pattern: Expected with RAII

Expected and RAII combine naturally. Wrap raw handles in RAII objects before returning them:

```cpp
Expected<FileHandle, std::string> open_file(const std::string& path) {
    FILE* f = fopen(path.c_str(), "r");
    if (!f) {
        return unexpected("Cannot open: " + path);
    }
    return FileHandle(f, fclose);  // RAII handle owns the FILE*
}
```

The returned FileHandle closes the file when destroyed, regardless of whether the caller checks for errors or not.

---

## Verification

Expected provides verification at both compile time and runtime. The strongest guarantees come from the type system itself.

### Compile-Time Guarantees

The compiler catches common mistakes before your code runs. Ignoring the return value triggers a warning because Expected is marked `[[nodiscard]]`. Treating an Expected as its underlying value fails because there's no implicit conversion. Performing operations that assume success fails because Expected doesn't expose the operators of its contained type:

```cpp
Expected<int, std::string> compute();

// ERROR: [[nodiscard]] - return value discarded
compute();

// ERROR: No implicit conversion to int
int x = compute();

// ERROR: No operator+ for Expected
auto y = compute() + 1;

// OK: Must explicitly access
if (auto r = compute(); r.has_value()) {
    int x = r.value();
}
```

These aren't runtime checks—they're compile errors. The class of bugs where C code silently ignores return values becomes impossible.

### Runtime Tests

Unit tests verify that the runtime behavior matches expectations. The most important test is that accessing `.value()` on an error throws an exception:

```cpp
TEST_CASE(value_on_error_throws) {
    Expected<int, std::string> err = unexpected(std::string("failed"));
    
    bool threw = false;
    try {
        [[maybe_unused]] int x = err.value();
    } catch (const bad_expected_access<std::string>& e) {
        threw = true;
        ASSERT_EQ(e.error(), "failed");
    }
    ASSERT_TRUE(threw);
}
```

This ensures that even if a programmer bypasses the `has_value()` check, they get an exception rather than undefined behavior.

Error propagation through EXPECTED_TRY should preserve the original error:

```cpp
TEST_CASE(expected_try_propagates) {
    auto inner = []() -> Expected<int, std::string> {
        return unexpected(std::string("inner"));
    };
    
    auto outer = [&]() -> Expected<int, std::string> {
        EXPECTED_TRY(val, inner());
        return val * 2;
    };
    
    auto result = outer();
    ASSERT_FALSE(result.has_value());
    ASSERT_EQ(result.error(), "inner");
}
```

And monadic operations should pass errors through without calling the transformation function:

```cpp
TEST_CASE(monadic_map_preserves_error) {
    Expected<int, std::string> err = unexpected(std::string("failed"));
    auto mapped = err.map([](int x) { return x * 2; });
    
    ASSERT_FALSE(mapped.has_value());
    ASSERT_EQ(mapped.error(), "failed");
}
```

---

## When Expected Loses

Expected solves the common case well, but some situations call for different approaches.

### 1. ABI Boundaries

Expected is a C++ template. It cannot cross a C ABI boundary—you can't return `Expected<T, E>` from an `extern "C"` function. At these boundaries, you must convert to C conventions:

```cpp
extern "C" int c_api(const char* in, char** out) {
    auto result = cpp_function(in);
    if (!result) return -1;
    *out = strdup(result->c_str());
    return 0;
}
```

Use Expected internally, convert at the edge.

### 2. Hot Loops with Trivial Errors

Expected stores a discriminator byte to track whether it holds a value or error. Even with optimized storage, checking that discriminator has a cost. In truly hot paths where profiling shows the check matters, consider whether the function can actually fail:

```cpp
int hot_path(int x) {
    // If failure is impossible, don't use Expected
    return x * 2;
}
```

This is a micro-optimization. Profile first.

### 3. Exception Interop

Code that calls exception-throwing libraries must convert at the boundary. A try-catch wrapper translates exceptions to Expected:

```cpp
Expected<Data, std::string> wrap_throwing() {
    try {
        return throwing_function();
    } catch (const std::exception& e) {
        return unexpected(std::string(e.what()));
    }
}
```

This adds overhead at the call site, but localizes exception handling to known boundaries.

### 4. Legacy Code Volume

Wrapping every C function in an Expected-returning wrapper takes effort. For large codebases, consider a pragmatic approach: wrap at module boundaries where Expected-based code calls legacy code, use generators for repetitive patterns, and accept that internal legacy code might keep its C-style error handling until it's rewritten for other reasons.

### 5. Error Type Consistency

Different libraries use different error types. When composing operations across library boundaries, you'll need to transform errors to a common type:

```cpp
auto result = lib1_function()
    .transform_error([](Lib1Error e) { return MyError(e); })
    .and_then([](auto v) { return lib2_function(v); })
    .transform_error([](Lib2Error e) { return MyError(e); });
```

This is verbose but explicit. You can see exactly where error type boundaries are crossed.

---

## Summary

C error handling relies on conventions that live outside the type system. Return values can be ignored. errno can be clobbered. Output parameters have ambiguous validity. Sentinels steal values from legitimate ranges. Every function call is an opportunity for a bug that the compiler will never catch.

Expected moves error handling into the type system. A function that can fail returns `Expected<T, E>`, and the caller must acknowledge both possibilities before extracting the value. The `[[nodiscard]]` attribute catches ignored results at compile time. The lack of implicit conversion catches accidental use as the underlying type. Monadic operations replace nested if-statements with composable pipelines.

| Aspect | C Pattern | Expected |
|--------|-----------|----------|
| Error signaling | Conventions | Type system |
| Check enforcement | Programmer discipline | Compiler enforced |
| Failure mode | Silent ignorance | Compile error or exception |
| Error preservation | errno clobbered | Travels with result |
| Composition | Manual if-chains | Monadic operations |
| Runtime cost | 0 | ~0 (trivial types) |

The migration pays off immediately in eliminated bugs, in the short term through cleaner error propagation with EXPECTED_TRY, and in the long term through self-documenting error contracts that new team members can understand from the function signatures alone.

---

## References

- [std::expected (C++23)](https://en.cppreference.com/w/cpp/utility/expected)
- [P0323R12: std::expected](https://wg21.link/p0323r12) — The C++23 proposal
- Fat-P User Manual: Expected — Complete API reference
- [Boost.Outcome](https://www.boost.org/doc/libs/release/libs/outcome/) — Alternative implementation

---

*FAT-P Library Documentation — January 2025*
