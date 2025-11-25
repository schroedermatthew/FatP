# Expected<T, E> User Manual

**Version:** 4.1  
**Library:** fat_p C++ Utilities  
**Standard:** C++17 (C++20/23 enhanced)  
**Type:** Header-only

---

## Table of Contents

1. [Overview](#overview)
2. [Key Features](#key-features)
3. [Quick Start](#quick-start)
4. [Core Concepts](#core-concepts)
5. [API Reference](#api-reference)
6. [Monadic Operations](#monadic-operations)
7. [Storage Policies](#storage-policies)
8. [Error Handling Patterns](#error-handling-patterns)
9. [C++20/23 Integration](#cpp2023-integration)
10. [Performance Considerations](#performance-considerations)
11. [Best Practices](#best-practices)
12. [Migration Guide](#migration-guide)

---

## Overview

`Expected<T, E>` is a vocabulary type for functions that may fail, providing a type-safe alternative to exceptions. It either contains a value of type `T` (success) or an error of type `E` (failure).

### Why Use Expected?

| Approach | Pros | Cons |
|----------|------|------|
| **Exceptions** | Natural syntax, automatic propagation | Runtime overhead, hard to reason about |
| **Error codes** | Fast, explicit | Easy to ignore, pollutes return type |
| **Expected** | Fast, explicit, composable, hard to ignore | Slightly verbose |

### Include

```cpp
#include "Expected.h"
```

### Namespace

```cpp
using namespace fat_p;
// Or use qualified names: fat_p::Expected<int, std::string>
```

---

## Key Features

- **Complete monadic interface**: `map`, `and_then`, `or_else`, `transform_error`
- **Lazy defaults**: `value_or_else()` for deferred computation
- **Storage policies**: `UnionStorage` (default) or `VariantStorage` (debug)
- **C++17 compatible**: Full functionality without newer standards
- **C++20 enhanced**: Three-way comparison (`operator<=>`)
- **C++23 integration**: Interoperability with `std::expected`
- **Strong exception guarantee**: All operations are exception-safe
- **Zero-overhead**: Optimized for performance-critical code

---

## Quick Start

### Basic Usage

```cpp
#include "Expected.h"
using namespace fat_p;

// Function that may fail
Expected<int, std::string> parse_int(const std::string& s) {
    try {
        return std::stoi(s);  // Implicit conversion to Expected
    } catch (const std::exception& e) {
        return make_unexpected(e.what());  // Return error
    }
}

// Using the result
auto result = parse_int("42");
if (result) {
    std::cout << "Value: " << *result << "\n";
} else {
    std::cout << "Error: " << result.error() << "\n";
}

// Or with value_or
int value = parse_int("invalid").value_or(-1);  // Returns -1
```

### Chaining Operations

```cpp
Expected<double, std::string> compute(const std::string& input) {
    return parse_int(input)
        .map([](int x) { return x * 2; })           // Transform value
        .map([](int x) { return static_cast<double>(x) / 3.0; })
        .transform_error([](auto& e) {              // Transform error
            return "Computation failed: " + e;
        });
}
```

---

## Core Concepts

### Creating Expected Values

```cpp
// Success cases
Expected<int, std::string> a = 42;                    // Implicit conversion
Expected<int, std::string> b{42};                     // Direct construction
Expected<int, std::string> c = Expected<int, std::string>(42);
auto d = make_expected<std::string>(42);              // Factory function

// In-place construction (avoids copy/move)
Expected<std::vector<int>, std::string> vec{std::in_place, 10, 0};

// Error cases
Expected<int, std::string> e = make_unexpected("error");
Expected<int, std::string> f{unexpect, "error"};      // In-place error

// Void value type
Expected<void, std::string> g;                        // Success (void)
Expected<void, std::string> h = make_unexpected("failed");
```

### Accessing Values

```cpp
Expected<int, std::string> result = get_value();

// Check state
if (result.has_value()) { /* ... */ }
if (result) { /* ... */ }                // Same as has_value()
if (!result) { /* error state */ }

// Access value (throws bad_expected_access if error)
int v1 = result.value();
int v2 = *result;                        // Same as value()

// Access error (undefined behavior if has value)
std::string err = result.error();

// Safe access with default
int v3 = result.value_or(0);
int v4 = result.value_or_else([]{ return compute_default(); });

// Pointer-like access
result->some_method();                   // Only if T has methods
```

### The `unexpected` Wrapper

```cpp
// Create error values
auto err1 = make_unexpected("error message");
auto err2 = unexpected<std::string>("error message");

// Use with Expected
Expected<int, std::string> result = make_unexpected("failed");

// In-place error construction
Expected<int, std::string> result2{unexpect, "constructed in place"};
```

---

## API Reference

### Type Aliases

```cpp
// Default error type is std::string
template <typename T>
using Result = Expected<T, std::string>;

// Void specialization
using Status = Expected<void, std::string>;
```

### Constructors

| Constructor | Description |
|-------------|-------------|
| `Expected()` | Default constructs value (requires T default-constructible) |
| `Expected(const T&)` | Copy constructs value |
| `Expected(T&&)` | Move constructs value |
| `Expected(const Expected&)` | Copy constructor |
| `Expected(Expected&&)` | Move constructor |
| `Expected(std::in_place_t, Args...)` | In-place value construction |
| `Expected(unexpect_t, Args...)` | In-place error construction |
| `Expected(const unexpected<E>&)` | Construct from unexpected |

### Observers

| Method | Returns | Description |
|--------|---------|-------------|
| `has_value()` | `bool` | True if contains value |
| `operator bool()` | `bool` | Same as `has_value()` |
| `value()` | `T&` | Value or throws `bad_expected_access` |
| `operator*()` | `T&` | Same as `value()` |
| `operator->()` | `T*` | Pointer to value |
| `error()` | `E&` | Error (UB if has value) |
| `value_or(U&&)` | `T` | Value or default |
| `value_or_else(F)` | `T` | Value or invoke F |
| `error_or(U&&)` | `E` | Error or default |

### Modifiers

| Method | Description |
|--------|-------------|
| `emplace(Args...)` | Construct value in-place |
| `swap(Expected&)` | Swap contents |

---

## Monadic Operations

### map (transform)

Transforms the value if present, leaving errors unchanged.

```cpp
Expected<int, std::string> x = 5;
Expected<double, std::string> y = x.map([](int n) { 
    return n * 2.5; 
});
// y contains 12.5

Expected<int, std::string> err = make_unexpected("error");
auto z = err.map([](int n) { return n * 2; });
// z still contains "error"
```

### and_then (flatMap/bind)

Chains operations that return Expected.

```cpp
Expected<int, std::string> parse(const std::string& s);
Expected<double, std::string> validate(int n);

auto result = parse("42")
    .and_then(validate)
    .and_then([](double d) -> Expected<double, std::string> {
        if (d < 0) return make_unexpected("negative");
        return d;
    });
```

### or_else

Handles errors, potentially recovering.

```cpp
Expected<int, std::string> result = get_from_cache()
    .or_else([](const std::string& err) {
        log_error(err);
        return get_from_database();  // Try alternative
    });
```

### transform_error

Transforms the error if present.

```cpp
Expected<int, std::string> result = get_value()
    .transform_error([](const std::string& e) {
        return "Wrapped: " + e;
    });
```

### inspect / inspect_error

Non-consuming observation (doesn't modify).

```cpp
auto result = compute()
    .inspect([](const auto& v) { log_value(v); })
    .inspect_error([](const auto& e) { log_error(e); });
```

### Chaining Example

```cpp
Expected<User, Error> load_user(int id) {
    return fetch_user_data(id)
        .and_then(parse_json)
        .and_then(validate_user)
        .map(create_user_object)
        .transform_error([id](auto& e) {
            return Error{"Failed to load user " + std::to_string(id), e};
        })
        .inspect([](const User& u) {
            metrics::record_user_load(u.id);
        });
}
```

---

## Storage Policies

### UnionStorage (Default)

- Uses `union` for optimal memory layout
- Zero overhead compared to manual union
- Requires careful lifetime management (handled automatically)

```cpp
Expected<int, std::string> x;  // Uses UnionStorage
```

### VariantStorage (Debug)

- Uses `std::variant` internally
- Better debugging support
- Slightly more overhead

```cpp
// Enable via preprocessor
#define USE_VARIANT_STORAGE
#include "Expected.h"
```

### Custom Storage

```cpp
template <typename T, typename E>
using DebugExpected = ExpectedImpl<T, E, VariantStorage>;

template <typename T, typename E>
using FastExpected = ExpectedImpl<T, E, UnionStorage>;
```

---

## Error Handling Patterns

### Pattern 1: Early Return

```cpp
Expected<Result, Error> process(Input input) {
    auto step1 = do_step1(input);
    if (!step1) return step1.error();
    
    auto step2 = do_step2(*step1);
    if (!step2) return step2.error();
    
    return do_step3(*step2);
}
```

### Pattern 2: Monadic Chaining

```cpp
Expected<Result, Error> process(Input input) {
    return do_step1(input)
        .and_then(do_step2)
        .and_then(do_step3);
}
```

### Pattern 3: TRY Macro (if defined)

```cpp
#define TRY(expr) \
    ({ auto&& _r = (expr); if (!_r) return _r.error(); *_r; })

Expected<Result, Error> process(Input input) {
    auto v1 = TRY(do_step1(input));
    auto v2 = TRY(do_step2(v1));
    return do_step3(v2);
}
```

### Pattern 4: Collecting Errors

```cpp
std::vector<Error> errors;
std::vector<Result> results;

for (const auto& item : items) {
    auto result = process(item);
    if (result) {
        results.push_back(*result);
    } else {
        errors.push_back(result.error());
    }
}
```

---

## C++20/23 Integration

### Three-Way Comparison (C++20)

```cpp
Expected<int, std::string> a = 5;
Expected<int, std::string> b = 10;

auto cmp = a <=> b;  // std::strong_ordering::less
```

### std::expected Interop (C++23)

```cpp
// Convert to std::expected
std::expected<int, std::string> std_exp = result.to_std_expected();

// Convert from std::expected
auto fat_p_exp = Expected<int, std::string>::from_std_expected(std_exp);
```

### Feature Detection

```cpp
#if defined(__cpp_utilities_expected_monadic)
    // Use monadic operations
#endif

#if defined(__cpp_utilities_expected_spaceship)
    // Use three-way comparison
#endif
```

---

## Performance Considerations

### Memory Layout

```cpp
// Expected<T, E> size = max(sizeof(T), sizeof(E)) + 1 byte (discriminant)
// Plus alignment padding

static_assert(sizeof(Expected<int, int>) == 8);  // Typical
static_assert(sizeof(Expected<char, char>) == 2);
```

### Move Semantics

```cpp
// Prefer move for large types
Expected<std::vector<int>, std::string> get_data();

auto result = get_data();
if (result) {
    auto data = std::move(*result);  // Move out value
}
```

### Avoid Copies in Chains

```cpp
// Good: values moved through chain
result.map([](auto&& x) { return transform(std::move(x)); });

// Bad: unnecessary copies
result.map([](auto x) { return transform(x); });
```

---

## Best Practices

### Do

```cpp
// ✅ Use for functions that can fail
Expected<File, IoError> open_file(const std::string& path);

// ✅ Use monadic operations for clean chaining
auto result = parse(input).and_then(validate).map(transform);

// ✅ Provide meaningful error types
enum class ParseError { InvalidSyntax, UnexpectedEof, InvalidToken };
Expected<Ast, ParseError> parse(const std::string& code);

// ✅ Use value_or_else for expensive defaults
auto value = result.value_or_else([] { return compute_expensive_default(); });
```

### Don't

```cpp
// ❌ Don't use for non-fallible functions
Expected<int, std::string> add(int a, int b);  // Just return int

// ❌ Don't ignore the result
get_value();  // Discarded Expected - bug!

// ❌ Don't use error() without checking
auto err = result.error();  // UB if has_value()!

// ❌ Don't nest Expected unnecessarily
Expected<Expected<int, E1>, E2>  // Confusing - flatten with and_then
```

---

## Migration Guide

### From Exceptions

```cpp
// Before
int parse(const std::string& s) {
    if (s.empty()) throw std::invalid_argument("empty");
    return std::stoi(s);
}

// After
Expected<int, std::string> parse(const std::string& s) {
    if (s.empty()) return make_unexpected("empty");
    try {
        return std::stoi(s);
    } catch (const std::exception& e) {
        return make_unexpected(e.what());
    }
}
```

### From Error Codes

```cpp
// Before
bool read_file(const std::string& path, std::string& out, int& error);

// After
Expected<std::string, int> read_file(const std::string& path);
```

### To std::expected (C++23)

```cpp
// Your code using fat_p::Expected continues to work
// For interop with std::expected APIs:
auto std_result = fat_p_result.to_std_expected();
```

---

## Thread Safety

`Expected` objects are **not** thread-safe for concurrent modification.

| Operation | Thread Safety |
|-----------|---------------|
| Multiple readers | ✅ Safe |
| Single writer | ✅ Safe |
| Concurrent writes | ❌ Unsafe |
| Read + Write | ❌ Unsafe |

For thread-safe access, use external synchronization:

```cpp
std::shared_mutex mtx;
Expected<Data, Error> shared_result;

// Read
{
    std::shared_lock lock(mtx);
    if (shared_result) { use(*shared_result); }
}

// Write
{
    std::unique_lock lock(mtx);
    shared_result = compute();
}
```

---

## Related Components

- **enforce.h**: `enforce_expected()` macro returns `Expected<void, std::string>`
- **CheckedArithmetic.h**: Returns `Expected<T, MathError>` with `ReturnExpectedPolicy`
- **PipeOperator.h**: `operator|` overloads for Expected chaining

---

**Document Version:** 1.0  
**Last Updated:** November 2025
