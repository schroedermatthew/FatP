---
doc_id: UM-EXPECTED-ASYNC-001
doc_type: "User Manual"
title: "ExpectedAsyncTask"
fatp_components: ["ExpectedAsyncTask", "Expected"]
topics: ["async", "Expected", "std::future", "poll", "then", "error handling"]
constraints: ["include only when async is needed", "poll returns optional", "std::future-backed"]
cxx_standard: "C++20"
last_verified: "2026-06-13"
audience: ["C++ developers", "AI assistants"]
status: "new"
---

# User Manual - ExpectedAsyncTask

`ExpectedAsyncTask.h` provides a small `std::future`-backed async wrapper for callables that return `fat_p::Expected<T, E>`.

It is intentionally separate from `Expected.h` so the core result type does not pull in async dependencies.

## Header

```cpp
#include "ExpectedAsyncTask.h"
```

This header includes `Expected.h` plus async-related standard headers.

## Basic use

```cpp
auto task = fat_p::async_task([]() -> fat_p::Expected<int, std::string> {
    return 42;
});

auto result = task.wait();
if (result) {
    use(*result);
}
```

## Polling

```cpp
auto maybe = task.poll();
if (!maybe) {
    // not ready
} else if (!*maybe) {
    // completed with domain error
    handle(maybe->error());
} else {
    // completed successfully
    use(**maybe);
}
```

`poll()` returns `std::optional<Expected<T, E>>`.

- `std::nullopt`: task is not ready
- `Expected<T, E>`: task completed

This keeps scheduler/task state separate from domain errors.

## Continuations

```cpp
auto next = fat_p::async_task([]() -> fat_p::Expected<int, std::string> {
        return 21;
    })
    .then([](int x) -> fat_p::Expected<int, std::string> {
        return x * 2;
    });
```

The continuation runs only if the previous task completed successfully. Errors propagate.

For `AsyncTask<void, E>`, the continuation takes no success argument:

```cpp
auto next = fat_p::async_task([]() -> fat_p::Expected<void, std::string> {
        return {};
    })
    .then([]() -> fat_p::Expected<int, std::string> {
        return 1;
    });
```

## Error observation

```cpp
auto observed = fat_p::async_task(work)
    .error([](const std::string& e) {
        log(e);
    });
```

The error handler observes the error and the task remains an `AsyncTask<T, E>`.

## Practical notes

`AsyncTask` is a convenience wrapper, not a full scheduler. It uses `std::async(std::launch::async, ...)` internally.

If the callable itself throws instead of returning `Expected<T, E>`, the exception follows normal `std::future` behavior and is rethrown by `wait()`. Prefer returning `unexpected<E>` for recoverable domain errors.

