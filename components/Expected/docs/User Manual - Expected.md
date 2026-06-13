---
doc_id: UM-EXPECTED-001
doc_type: "User Manual"
title: "Expected"
fatp_components: ["Expected", "ExpectedAsyncTask"]
topics: ["error handling", "Expected type", "monadic operations", "error propagation", "EXPECTED_TRY", "sum type", "value or error", "railway programming", "storage policies", "void Expected", "AsyncTask"]
constraints: ["C++20 minimum", "explicit recoverable errors", "storage policy selection", "async split header"]
cxx_standard: "C++20"
std_equivalent: "std::expected<T, E>"
std_since: "C++23"
build_modes: ["Debug", "Release"]
last_verified: "2026-06-13"
audience: ["C++ developers", "AI assistants"]
status: "updated"
---

# User Manual - Expected

## Scope

This manual covers `fat_p::Expected<T, E>`, `fat_p::Expected<void, E>`, storage policies, monadic operations, early-return macros, C++23 `std::expected` conversion helpers, and the split async wrapper in `ExpectedAsyncTask.h`.

This manual does not cover the project-wide contract system. Use `Expected` for recoverable domain failures. Use contracts/assertions/enforcement for caller violations and impossible states.

## Headers

Use the core result type with:

```cpp
#include "Expected.h"
```

Use async support only where needed:

```cpp
#include "ExpectedAsyncTask.h"
```

`ExpectedAsyncTask.h` includes `Expected.h` and then adds `<future>`, `<optional>`, and async task support. Keeping it separate prevents all normal `Expected` users from paying for async dependencies.

## Quick start

```cpp
#include "Expected.h"

fat_p::Expected<int, std::string> parse_int(std::string_view text)
{
    if (text.empty()) {
        return fat_p::unexpected<std::string>("empty input");
    }

    // Example only.
    return 42;
}

void example()
{
    auto value = parse_int("42");
    if (!value) {
        log(value.error());
        return;
    }

    use(*value);
}
```

## Core vocabulary

### `Expected<T, E>`

Holds either:

- a success value of type `T`, or
- an error value of type `E`.

```cpp
fat_p::Expected<int, std::string> ok = 7;
fat_p::Expected<int, std::string> err = fat_p::unexpected<std::string>("bad input");
```

### `Expected<void, E>`

Represents success with no value, or an error.

```cpp
fat_p::Expected<void, std::string> save();
```

### `Result<T>` and `Status`

Convenience aliases:

```cpp
fat_p::Result<int>  // Expected<int, std::string>
fat_p::Status       // Expected<void, std::string>
```

Use domain-specific error types when they are better than strings:

```cpp
enum class ParseError { EmptyInput, InvalidDigit, Overflow };
fat_p::Expected<int, ParseError> parse_int(std::string_view text);
```

### `unexpected<E>`

Use `unexpected` to construct an error state unambiguously:

```cpp
return fat_p::unexpected<ParseError>(ParseError::InvalidDigit);
```

or:

```cpp
return fat_p::make_unexpected(ParseError::InvalidDigit);
```

## Basic access

```cpp
auto r = compute();

if (r.has_value()) {
    use(*r);
}

if (!r) {
    handle(r.error());
}
```

Available observers:

```cpp
r.has_value()
r.has_error()
static_cast<bool>(r)
```

Available value accessors:

```cpp
*r
r->member
r.value()
r.value_or(default_value)
r.value_or_else([] { return expensive_default(); })
```

Available error accessors:

```cpp
r.error()
r.error_or(default_error)
r.error_or_else([] { return make_error(); })
```

### Checked and unchecked access

`value()` checks the state and throws `bad_expected_access<E>` when the object holds an error. In no-exception builds, this path terminates through the configured no-exception behavior.

`operator*`, `operator->`, `error()`, and `value_unchecked()` are for already-verified states. Do not call them on the wrong state.

Use this pattern:

```cpp
if (r) {
    use(*r);          // OK: state was checked
}
```

Do not use this pattern:

```cpp
use(*r);              // wrong unless the state is known by contract
```

## Returning errors

```cpp
fat_p::Expected<double, std::string> reciprocal(double x)
{
    if (x == 0.0) {
        return fat_p::unexpected<std::string>("division by zero");
    }
    return 1.0 / x;
}
```

When `T` and `E` are the same type, `unexpected` is required to disambiguate the error state:

```cpp
fat_p::Expected<int, int> good = 1;
fat_p::Expected<int, int> bad = fat_p::unexpected<int>(-1);
```

## Monadic operations

Monadic operations let you build pipelines without manually checking after every step.

### `map`

Transforms the success value. Errors pass through unchanged.

```cpp
auto doubled = parse_int(text).map([](int x) {
    return x * 2;
});
```

If the mapper returns `void`, the result type becomes `Expected<void, E>`:

```cpp
auto logged = parse_int(text).map([](int x) {
    log(x);
});
```

### `and_then`

Chains a function that itself returns `Expected`.

```cpp
auto result = read_file(path)
    .and_then(parse_json)
    .and_then(decode_config);
```

The callback should return a Fat-P `Expected` / `ExpectedImpl` with the same error type.

### `or_else`

Recovers from an error by returning another `Expected` with the same value type.

```cpp
auto cfg = read_config(primary)
    .or_else([&](const Error&) {
        return read_config(fallback);
    });
```

### `transform_error` / `map_error`

Transforms the error type while leaving successes alone.

```cpp
auto normalized = parse_int(text).transform_error([](ParseError e) {
    return to_string(e);
});
```

### `inspect` and `inspect_error`

Observe success or error without changing the value.

```cpp
return load(path)
    .inspect([](const Config& cfg) { log_loaded(cfg); })
    .inspect_error([](const Error& e) { log_error(e); });
```

### `fold`

Collapse success and error into one return type.

```cpp
std::string message = result.fold(
    [](int value) { return "value=" + std::to_string(value); },
    [](const Error& e) { return "error=" + to_string(e); });
```

## Early-return macros

### `FATP_EXPECTED_TRY`

Use when the expression returns `Expected<T, E>` and you need the success value.

```cpp
fat_p::Expected<Config, Error> load_config(Path path)
{
    FATP_EXPECTED_TRY(bytes, read_file(path));
    FATP_EXPECTED_TRY(json, parse_json(bytes));
    return decode_config(json);
}
```

If `read_file` fails, the enclosing function returns the error immediately. Otherwise `bytes` is bound to the success value.

### `FATP_EXPECTED_TRY_VOID`

Use when the expression returns `Expected<void, E>`.

```cpp
fat_p::Status save_all()
{
    FATP_EXPECTED_TRY_VOID(open_output());
    FATP_EXPECTED_TRY_VOID(write_header());
    FATP_EXPECTED_TRY_VOID(write_payload());
    return {};
}
```

### `FATP_EXPECTED_ASSIGN_OR_RETURN`

Use when the destination variable already exists.

```cpp
int id = 0;
FATP_EXPECTED_ASSIGN_OR_RETURN(id, parse_id(text));
```

## Storage policies

### Default `Expected<T, E>`

```cpp
template <typename T, typename E = std::string>
using Expected = ExpectedImpl<T, E, FATP_DEFAULT_STORAGE>;
```

Unless configured otherwise, `FATP_DEFAULT_STORAGE` is `UnionStorage`.

### `UnionStorage`

Manual union storage. This is the default because it avoids `std::variant` overhead and keeps the representation close to a tagged union.

Use explicitly when you want the default behavior regardless of macro configuration:

```cpp
fat_p::ExpectedUnion<int, Error> r;
```

### `VariantStorage`

When `USE_VARIANT_STORAGE` is defined before including the header, the default storage policy becomes `VariantStorage` and `ExpectedVariant<T, E>` is available.

```cpp
#define USE_VARIANT_STORAGE
#include "Expected.h"
```

`VariantStorage` is intended for debug/non-hot-path use. It relies on `std::variant`, has a larger dependency/overhead profile, and is not the default.

### `TrivialStorage` and `TrivialExpected`

`TrivialExpected<T, E>` is for trivially copyable `T` and `E`:

```cpp
fat_p::TrivialExpected<int, int> fast_compute(int x)
{
    if (x < 0) {
        return fat_p::unexpected<int>(-1);
    }
    return x * 2;
}
```

Use it only when the ABI/performance tradeoff matters and both value and error are simple types. For normal domain errors, use `Expected<T, E>`.

## Void Expected

`Expected<void, E>` represents success/failure without a success payload.

```cpp
fat_p::Expected<void, Error> validate(const Config& cfg)
{
    if (!cfg.valid()) {
        return fat_p::unexpected<Error>(Error::InvalidConfig);
    }
    return {};
}
```

It supports the same general flow:

```cpp
return validate(cfg)
    .and_then([&] { return save(cfg); })
    .or_else([](Error e) { return recover(e); });
```

## C++23 `std::expected` interoperability

When the standard library provides `std::expected`, conversion helpers are available:

```cpp
std::expected<int, Error> std_r = fat_p::to_std_expected(fatp_r);
fat_p::Expected<int, Error> fatp_r2 = fat_p::from_std_expected(std_r);
```

These helpers are for boundary interop. Inside Fat-P pipelines, monadic callbacks should return Fat-P `Expected` types.

## AsyncTask

Async support is separate:

```cpp
#include "ExpectedAsyncTask.h"
```

Create a task from a callable that returns `Expected<T, E>`:

```cpp
auto task = fat_p::async_task([]() -> fat_p::Expected<int, std::string> {
    return 42;
});
```

Wait for completion:

```cpp
auto result = task.wait();
if (result) {
    use(*result);
}
```

Poll without blocking:

```cpp
std::optional<fat_p::Expected<int, std::string>> maybe_result = task.poll();

if (!maybe_result) {
    // not ready yet
} else if (!*maybe_result) {
    // completed with domain error
    log(maybe_result->error());
} else {
    // completed successfully
    use(**maybe_result);
}
```

`poll()` does not synthesize an error value for “not ready.” Not-ready is task state, represented by `std::nullopt`.

Chain continuations:

```cpp
auto next = fat_p::async_task([]() -> fat_p::Expected<int, std::string> {
        return 21;
    })
    .then([](int x) -> fat_p::Expected<int, std::string> {
        return x * 2;
    });
```

For `AsyncTask<void, E>`, the continuation takes no success argument:

```cpp
auto next = fat_p::async_task([]() -> fat_p::Expected<void, std::string> {
        return {};
    })
    .then([]() -> fat_p::Expected<int, std::string> {
        return 7;
    });
```

## Thread safety

Individual `Expected` objects are not thread-safe for concurrent mutation.

Safe:

- multiple threads reading the same immutable `Expected`
- moving/copying separate `Expected` objects under normal C++ object rules

Unsafe without synchronization:

- one thread reading while another mutates the same `Expected`
- multiple writers on the same object

For shared mutable state, use external synchronization. Do not assume `std::atomic<Expected<T, E>>` works for general `Expected`. Atomic use is only possible for complete instantiations that satisfy the standard atomic requirements, such as selected trivial cases.

## Unsupported or intentionally excluded

### `Expected<T&, E>`

Reference value types are intentionally unsupported. Use a pointer, `std::reference_wrapper`, or a domain-specific non-owning wrapper if that is the actual API contract.

```cpp
fat_p::Expected<std::reference_wrapper<T>, Error> find_ref();
fat_p::Expected<T*, Error> find_ptr();
```

### C++17

Fat-P requires C++20 or later. The header relies on C++20 features and `CppFeatureDetection.h` enforces the standard level.

### Automatic storage selection

There is no `AutoStorage` policy in this implementation. Storage choice is explicit through aliases/macros.

## Recommended usage patterns

### Use typed errors for library APIs

```cpp
enum class FileError {
    NotFound,
    PermissionDenied,
    InvalidEncoding
};

fat_p::Expected<std::string, FileError> read_text(Path path);
```

### Use `Status` for simple application plumbing

```cpp
fat_p::Status initialize();
fat_p::Status shutdown();
```

### Convert at boundaries

If a third-party API expects C++23 `std::expected`, convert at the boundary and keep Fat-P `Expected` internally if you need storage policies or Fat-P macros.

### Keep `Expected.h` in core headers, `ExpectedAsyncTask.h` in implementation/async headers

This preserves the dependency split.

## Common mistakes

### Calling `.value()` as a habit

Prefer explicit checks or monadic composition. Use `.value()` when throwing/terminating on error is genuinely the desired behavior.

### Returning `unexpected` without specifying the intended error type

This can work through deduction, but explicit error types make APIs clearer, especially when `T` and `E` are convertible.

```cpp
return fat_p::unexpected<MyError>(MyError::Invalid);
```

### Using `Expected` for contract violations

Do not return `Expected` for bugs like “caller passed an out-of-range index” if the component's contract says that is invalid. Use the project's contract mechanism.

### Pulling async into every header

Do not include `ExpectedAsyncTask.h` from broad public headers unless async types are part of that public API.

## Minimal compile examples

Core:

```cpp
#include "Expected.h"

fat_p::Expected<int, int> f(bool ok)
{
    if (!ok) return fat_p::unexpected<int>(-1);
    return 1;
}
```

Async:

```cpp
#include "ExpectedAsyncTask.h"

fat_p::AsyncTask<int, std::string> start()
{
    return fat_p::async_task([]() -> fat_p::Expected<int, std::string> {
        return 42;
    });
}
```

