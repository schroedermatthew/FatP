# Expected: Explicit Results for Fat-P

## Executive summary

`fat_p::Expected<T, E>` is Fat-P's C++20 result type for functions that can either produce a value or return a typed error. It is intended for code where error flow should be visible at the call site and where exceptions are either disabled, undesirable across a boundary, or too implicit for the domain.

The core header is now intentionally small:

```cpp
#include "Expected.h"
```

It provides:

- `Expected<T, E>` and `Expected<void, E>`
- `Result<T>` and `Status` convenience aliases using `std::string` errors
- `unexpected<E>` / `make_unexpected`
- monadic operations: `map`, `and_then`, `or_else`, `transform_error`, `inspect`, `inspect_error`, and `fold`
- lazy fallbacks: `value_or_else` and `error_or_else`
- `FATP_EXPECTED_TRY`, `FATP_EXPECTED_TRY_VOID`, and `FATP_EXPECTED_ASSIGN_OR_RETURN`
- selectable storage policies: `UnionStorage`, `VariantStorage`, and `TrivialStorage`
- optional C++23 conversion helpers for `std::expected`

Async support has been split out so normal users of `Expected` do not pay for `<future>` and related dependencies:

```cpp
#include "ExpectedAsyncTask.h" // only when AsyncTask is needed
```

## What problem it solves

Exceptions hide error flow:

```cpp
Config load_config(const Path& path); // may throw; not visible in the type
```

Error codes separate success values from error state:

```cpp
int load_config(const Path& path, Config* out); // caller must coordinate two channels
```

`Expected` keeps the result and the error in one value:

```cpp
fat_p::Expected<Config, ConfigError> load_config(const Path& path);
```

A caller can then choose explicit branching:

```cpp
auto cfg = load_config(path);
if (!cfg) {
    log(cfg.error());
    return fat_p::unexpected(cfg.error());
}
use(*cfg);
```

or pipeline-style composition:

```cpp
return read_file(path)
    .and_then(parse_config)
    .and_then(validate_config)
    .map(build_runtime_config);
```

## Design principles

### 1. Visible failure at API boundaries

A function returning `Expected<T, E>` states in its signature that failure is expected and recoverable. This is different from contract violations, programmer errors, or invariant failures, which should use the project's contract/enforcement layer.

### 2. No async dependency in the core result type

The core `Expected.h` header does not include `ExpectedAsyncTask.h`. This keeps the foundation result type usable in low-dependency headers, hot paths, and build-sensitive code.

Use `ExpectedAsyncTask.h` only when you need the `std::future`-backed async wrapper.

### 3. Storage policy is explicit

The default `Expected<T, E>` uses the configured default storage policy. By default, that is `UnionStorage`.

Explicit aliases are available when the storage choice matters:

```cpp
fat_p::ExpectedUnion<T, E>      // force manual union storage
fat_p::TrivialExpected<T, E>    // trivially copyable hot-path form
// fat_p::ExpectedVariant<T, E> // available when USE_VARIANT_STORAGE is enabled
```

`USE_VARIANT_STORAGE` switches the default `Expected<T, E>` to `VariantStorage`. Variant storage is useful for debug or non-hot paths. It is not the default because it adds the overhead and dependency profile of `std::variant`.

### 4. `Expected<void, E>` is a status type

Use `Expected<void, E>` or `Status` when the only success information is “the operation completed.”

```cpp
fat_p::Status save_file(const Path& path, const Buffer& data);
```

### 5. `TrivialExpected` is for simple ABI-oriented hot paths

`TrivialExpected<T, E>` is intended for trivially copyable `T` and `E`, such as integer values and small error codes. It is designed for cases where passing a small result in registers matters.

Use it deliberately. For general application/domain errors, prefer normal `Expected<T, E>`.

## C++ standard support

Fat-P requires C++20. `Expected` uses C++20 language/library features such as concepts and three-way comparison support.

C++23 adds optional `std::expected` conversion helpers when the standard library provides `std::expected`:

```cpp
auto std_exp = fat_p::to_std_expected(my_expected);
auto fatp_exp = fat_p::from_std_expected(std_exp);
```

The conversion helpers are interop utilities. Monadic callbacks should return `fat_p::Expected` / `fat_p::ExpectedImpl` so error propagation uses Fat-P's `unexpected` tag and storage policies.

## What it is not

`Expected` is not an ownership type. It owns either the value or the error, but it does not solve lifetime issues for references. `Expected<T&, E>` is intentionally unsupported.

`Expected` is not a replacement for contract checks. Use it for recoverable domain failures, not for impossible states or caller contract violations.

`Expected` is not a blanket exception-elimination tool. It is best used at clear recoverable-error boundaries. Internal code may still use contracts, assertions, or exceptions if that is the component's chosen model.

## Example: explicit branching

```cpp
fat_p::Expected<int, std::string> divide(int a, int b)
{
    if (b == 0) {
        return fat_p::unexpected<std::string>("division by zero");
    }
    return a / b;
}

fat_p::Status run()
{
    auto result = divide(10, 2);
    if (!result) {
        return fat_p::unexpected(result.error());
    }

    use(*result);
    return {};
}
```

## Example: monadic pipeline

```cpp
fat_p::Expected<TokenStream, ParseError> tokenize(std::string_view text);
fat_p::Expected<Ast, ParseError> parse(TokenStream tokens);
fat_p::Expected<CheckedAst, ParseError> type_check(Ast ast);

fat_p::Expected<CheckedAst, ParseError> compile_frontend(std::string_view text)
{
    return tokenize(text)
        .and_then(parse)
        .and_then(type_check);
}
```

## Example: early-return macro

```cpp
fat_p::Expected<Config, ConfigError> load_config(const Path& path)
{
    FATP_EXPECTED_TRY(bytes, read_file(path));
    FATP_EXPECTED_TRY(json, parse_json(bytes));
    return decode_config(json);
}
```

`FATP_EXPECTED_TRY(name, expr)` evaluates `expr`, returns its error if it failed, and binds the success value to `name`.

For `Expected<void, E>` expressions, use:

```cpp
FATP_EXPECTED_TRY_VOID(flush_output());
```

## AsyncTask split

Async support now lives in `ExpectedAsyncTask.h`:

```cpp
#include "ExpectedAsyncTask.h"

auto task = fat_p::async_task([]() -> fat_p::Expected<int, std::string> {
    return 42;
});

auto ready = task.poll();
if (!ready) {
    // not ready yet
}
```

`poll()` returns `std::optional<Expected<T, E>>`:

- `std::nullopt` means the task is not ready
- `Expected<T, E>` means the task completed, either successfully or with a domain error

This avoids inventing a fake domain error such as `"Not ready"`.

## Current maturity

The core API is broad and intended for real use, but the right mental model is still practical rather than magical:

- keep `T` and `E` reasonably movable/copyable for the paths you use;
- do not use unchecked accessors unless the state was externally verified;
- use `TrivialExpected` only for trivially copyable hot-path types;
- include `ExpectedAsyncTask.h` only where async is required.

