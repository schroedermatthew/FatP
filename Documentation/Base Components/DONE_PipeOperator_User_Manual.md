# PipeOperator User Manual

## Table of Contents

1. [What is PipeOperator and Why Do You Need It?](#what-is-pipeoperator-and-why-do-you-need-it)
   - [The Function Composition Problem](#the-function-composition-problem)
   - [Inspiration from Rust and F#](#inspiration-from-rust-and-f)
   - [Where PipeOperator Fits](#where-pipeoperator-fits)
2. [Core Architecture](#core-architecture)
   - [Overload Resolution Strategy](#overload-resolution-strategy)
   - [Type Detection via SFINAE](#type-detection-via-sfinae)
   - [Map vs Bind Semantics](#map-vs-bind-semantics)
3. [Getting Started](#getting-started)
   - [Prerequisites](#prerequisites)
   - [Integration](#integration)
   - [First Pipeline](#first-pipeline)
4. [Basic Value Piping](#basic-value-piping)
   - [Simple Transformations](#simple-transformations)
   - [Type Conversions](#type-conversions)
   - [Multi-Stage Pipelines](#multi-stage-pipelines)
5. [Expected Piping](#expected-piping)
   - [Map Operation (T → U)](#map-operation-t--u)
   - [Bind Operation (T → Expected\<U\>)](#bind-operation-t--expectedu)
   - [Error Propagation](#error-propagation)
   - [Mixed Pipelines](#mixed-pipelines)
6. [Void Expected Support](#void-expected-support)
   - [Status to Value Pipelines](#status-to-value-pipelines)
   - [Void to Void Operations](#void-to-void-operations)
   - [Error Propagation from Status](#error-propagation-from-status)
7. [C++20 Ranges Compatibility](#c20-ranges-compatibility)
   - [The Conflict Problem](#the-conflict-problem)
   - [Using pipe() Wrapper](#using-pipe-wrapper)
   - [Scoped using Declarations](#scoped-using-declarations)
8. [Performance Characteristics](#performance-characteristics)
   - [Zero-Overhead Abstraction](#zero-overhead-abstraction)
   - [Benchmark Results](#benchmark-results)
   - [Compiler Optimization](#compiler-optimization)
9. [Comparison with Alternatives](#comparison-with-alternatives)
   - [PipeOperator vs Method Chaining](#pipeoperator-vs-method-chaining)
   - [PipeOperator vs Ranges](#pipeoperator-vs-ranges)
   - [PipeOperator vs Rust](#pipeoperator-vs-rust)
10. [Advanced Usage](#advanced-usage)
    - [Custom Expected Types](#custom-expected-types)
    - [Stateful Lambdas](#stateful-lambdas)
    - [Exception-Free Pipelines](#exception-free-pipelines)
11. [Integration with fat_p Components](#integration-with-fat_p-components)
12. [Troubleshooting](#troubleshooting)
    - [Common Issues](#common-issues)
    - [Compilation Errors](#compilation-errors)
    - [Runtime Issues](#runtime-issues)
13. [Best Practices](#best-practices)
14. [Design Decisions and Tradeoffs](#design-decisions-and-tradeoffs)
15. [Summary](#summary)

---

## What is PipeOperator and Why Do You Need It?

### The Function Composition Problem

Composing multiple functions in C++ traditionally requires either deeply nested calls or intermediate variables:

```cpp
// Nested calls: Hard to read, evaluation order unclear
auto result = format(validate(transform(parse(input))));

// Intermediate variables: Verbose, pollutes namespace
auto parsed = parse(input);
auto transformed = transform(parsed);
auto validated = validate(transformed);
auto result = format(validated);
```

This becomes worse with error handling:

```cpp
// Nested error handling: Pyramid of doom
auto parsed = parse(input);
if (!parsed.has_value()) return parsed.error();
auto transformed = transform(*parsed);
if (!transformed.has_value()) return transformed.error();
auto validated = validate(*transformed);
if (!validated.has_value()) return validated.error();
return format(*validated);
```

### Inspiration from Rust and F#

**Rust** solved this elegantly with the `?` operator for error propagation:

```rust
let result = parse(input)?
    .transform()?
    .validate()?
    .format();
```

**F#** and other functional languages use the pipe operator:

```fsharp
let result = input |> parse |> transform |> validate |> format
```

### Where PipeOperator Fits

**fat_p::PipeOperator** brings these concepts to C++17:

```cpp
// Clean, readable, left-to-right data flow
auto result = input 
    | parse 
    | transform 
    | validate 
    | format;

// With Expected: Automatic error propagation!
Expected<Config> result = read_file(path)
    | parse_json
    | validate_schema
    | transform_config;
// If any step fails, error propagates automatically
```

**When to use PipeOperator:**
- Data transformation pipelines (parse → validate → transform → output)
- Expected-based error handling with clean syntax
- Functional programming style in C++
- Any sequence of composable operations

**When NOT to use PipeOperator:**
- Already using C++20 Ranges extensively (potential conflicts)
- Need lazy evaluation (pipe is eager)
- Complex control flow with early returns (use EXPECTED_TRY instead)
- Performance-critical inner loops (profile first—usually fine)

---

## Core Architecture

### Overload Resolution Strategy

PipeOperator uses SFINAE (Substitution Failure Is Not An Error) to select the correct overload at compile time:

```mermaid
flowchart TD
    A[T | Func] --> B{Is T Expected?}
    B -->|No| C[General Pipe: func value]
    B -->|Yes| D{Is T void?}
    D -->|No| E{Returns Expected?}
    D -->|Yes| F{Returns Expected?}
    E -->|No| G[Map: wrap result]
    E -->|Yes| H[Bind: no wrapping]
    F -->|No| I[Void Map: wrap result]
    F -->|Yes| J[Void Bind: no wrapping]
```

### Type Detection via SFINAE

The key trait is `is_expected_v`, which detects whether a type is an `ExpectedImpl`:

```cpp
// In FatPTypeTraits.h
template <typename T>
struct is_expected : std::false_type {};

// Specialized in Expected.h for ExpectedImpl
template <typename T, typename E, template <typename, typename> class SP>
struct is_expected<ExpectedImpl<T, E, SP>> : std::true_type {};
```

Overloads use `std::enable_if_t` to match the correct case:

```cpp
// Only enabled when T is NOT Expected
template <typename T, typename Func>
auto operator|(T&& value, Func&& func) 
    -> std::enable_if_t<
        !is_expected_v<std::decay_t<T>>, 
        decltype(func(std::forward<T>(value)))
    >;

// Only enabled when func returns non-Expected
template <typename T, typename E, ...>
auto operator|(ExpectedImpl<T, E, Storage>&& exp, Func&& func)
    -> std::enable_if_t<
        !is_expected_v<std::decay_t<decltype(func(*exp))>>,
        ExpectedImpl<decltype(func(*exp)), E, Storage>
    >;
```

### Map vs Bind Semantics

The pipe operator automatically chooses between **map** and **bind** based on the function's return type:

| Function Returns | Operation | Result Type | Double-Wrapping? |
|-----------------|-----------|-------------|------------------|
| `U` (plain value) | Map | `Expected<U, E>` | N/A - wraps once |
| `Expected<U, E>` | Bind | `Expected<U, E>` | No - flattens |

**Map** (function returns plain value):
```cpp
Expected<int> exp(42);
auto result = exp | [](int x) { return x * 2; };  // Map
// result type: Expected<int>
// result value: 84
```

**Bind** (function returns Expected):
```cpp
Expected<int> exp(42);
auto result = exp | [](int x) -> Expected<int> { 
    if (x > 100) return unexpected("too large");
    return x * 2; 
};  // Bind
// result type: Expected<int> (NOT Expected<Expected<int>>)
```

---

## Getting Started

### Prerequisites

- C++17 or later
- `Expected.h` header
- `FatPTypeTraits.h` header

### Integration

```cpp
#include "Expected.h"
#include "PipeOperator.h"

using namespace fat_p;  // Or use fat_p:: prefix
```

### First Pipeline

```cpp
#include "Expected.h"
#include "PipeOperator.h"
#include <iostream>

int main() {
    using namespace fat_p;
    
    // Simple value pipeline
    auto add_ten = [](int x) { return x + 10; };
    auto multiply = [](int x) { return x * 2; };
    
    int result = 5 | add_ten | multiply;
    std::cout << result << "\n";  // Output: 30
    
    // Expected pipeline with error propagation
    auto parse = [](std::string s) -> Expected<int> {
        try { return std::stoi(s); }
        catch (...) { return unexpected("parse error"); }
    };
    auto validate = [](int x) -> Expected<int> {
        if (x < 0) return unexpected("negative");
        return x;
    };
    auto format = [](int x) { return "Value: " + std::to_string(x); };
    
    auto output = Expected<std::string>("42")
        | parse
        | validate
        | format;
    
    if (output) {
        std::cout << *output << "\n";  // Output: Value: 42
    }
    
    return 0;
}
```

---

## Basic Value Piping

### Simple Transformations

The general pipe operator works with any value and any callable:

```cpp
auto add_one = [](int x) { return x + 1; };
auto square = [](int x) { return x * x; };
auto to_string = [](int x) { return std::to_string(x); };

// Chain transformations left-to-right
int result = 3 | add_one | square;  // (3+1)^2 = 16
std::string text = 5 | square | to_string;  // "25"
```

### Type Conversions

Pipe handles type changes naturally:

```cpp
auto int_to_double = [](int x) { return static_cast<double>(x); };
auto double_to_string = [](double x) { 
    return std::to_string(x); 
};
auto string_length = [](std::string s) { return s.length(); };

size_t len = 42 
    | int_to_double 
    | double_to_string 
    | string_length;  // len = 11 (length of "42.000000")
```

### Multi-Stage Pipelines

Build complex transformations from simple steps:

```cpp
struct Point { double x, y; };

auto normalize = [](Point p) {
    double len = std::sqrt(p.x*p.x + p.y*p.y);
    return Point{p.x/len, p.y/len};
};

auto scale = [](double factor) {
    return [factor](Point p) {
        return Point{p.x * factor, p.y * factor};
    };
};

auto to_string = [](Point p) {
    return "(" + std::to_string(p.x) + ", " + std::to_string(p.y) + ")";
};

std::string result = Point{3.0, 4.0}
    | normalize           // (0.6, 0.8)
    | scale(10.0)        // (6.0, 8.0)
    | to_string;         // "(6.0, 8.0)"
```

---

## Expected Piping

### Map Operation (T → U)

When your function returns a plain value, pipe automatically wraps it in Expected:

```cpp
Expected<int> exp(42);

// Function returns int, result is Expected<int>
auto doubled = exp | [](int x) { return x * 2; };
// doubled: Expected<int>(84)

// Type conversion works too
auto text = exp | [](int x) { return std::to_string(x); };
// text: Expected<std::string>("42")
```

### Bind Operation (T → Expected\<U\>)

When your function returns Expected, pipe flattens the result:

```cpp
Expected<int> exp(42);

auto safe_divide = [](int x) -> Expected<int> {
    if (x == 0) return unexpected("division by zero");
    return 100 / x;
};

// Function returns Expected<int>, result is Expected<int> (not nested!)
auto result = exp | safe_divide;
// result: Expected<int>(2)

// Error case
Expected<int> zero(0);
auto fail = zero | safe_divide;
// fail: Expected<int> with error "division by zero"
```

### Error Propagation

Once a pipeline encounters an error, subsequent operations are skipped:

```cpp
auto step1 = [](int x) -> Expected<int> { 
    std::cout << "step1\n"; 
    return x + 10; 
};
auto step2 = [](int x) -> Expected<int> { 
    std::cout << "step2\n"; 
    return unexpected("error!"); 
};
auto step3 = [](int x) -> Expected<int> { 
    std::cout << "step3\n";  // Never printed!
    return x * 2; 
};

auto result = Expected<int>(5) | step1 | step2 | step3;
// Output:
// step1
// step2
// (step3 is skipped)
// result.error() == "error!"
```

### Mixed Pipelines

Combine map and bind operations freely:

```cpp
Expected<std::string> input("42");

auto parse = [](std::string s) -> Expected<int> {
    try { return std::stoi(s); }
    catch (...) { return unexpected("parse failed"); }
};

auto validate = [](int x) -> Expected<int> {
    if (x < 0 || x > 100) return unexpected("out of range");
    return x;
};

auto transform = [](int x) { return x * 2; };  // Plain return

auto format = [](int x) { return "Result: " + std::to_string(x); };

auto result = input
    | parse       // bind: string → Expected<int>
    | validate    // bind: int → Expected<int>
    | transform   // map: int → int (wrapped)
    | format;     // map: int → string (wrapped)

// result type: Expected<std::string>
```

---

## Void Expected Support

### Status to Value Pipelines

`Expected<void>` (aka `Status`) can pipe into value-producing functions:

```cpp
Expected<void> init_status = initialize_system();

auto get_config = []() { return Config{...}; };

auto result = init_status | get_config;
// If init succeeded: result is Expected<Config> with value
// If init failed: result is Expected<Config> with error propagated
```

With Expected-returning functions:

```cpp
Status init_status = initialize();

auto load_config = []() -> Expected<Config> {
    // Load configuration
    return Config{...};
};

auto result = init_status | load_config;
// result type: Expected<Config>
```

### Void to Void Operations

Chain side-effect operations:

```cpp
int step_count = 0;

Status status;

auto step1 = [&]() { step_count++; };
auto step2 = [&]() { step_count++; };
auto step3 = [&]() { step_count++; };

auto final = status | step1 | step2 | step3;
// final type: Expected<void>
// step_count == 3 if all succeeded
```

### Error Propagation from Status

Errors in void Expected propagate through the pipeline:

```cpp
Status failure(unexpected("initialization failed"));

auto step1 = []() { 
    std::cout << "step1\n";  // Never printed!
    return 42; 
};

auto result = failure | step1;
// result type: Expected<int>
// result.has_error() == true
// result.error() == "initialization failed"
```

Real-world example:

```cpp
auto result = initialize()     // Expected<void>
    | load_config              // () -> Expected<Config>
    | validate_config          // Config -> Expected<Config>
    | apply_config             // Config -> Expected<void>
    | []() { return "ready"; }; // () -> string
    
// result: Expected<std::string>
// Either contains "ready" or first error encountered
```

---

## C++20 Ranges Compatibility

### The Conflict Problem

C++20 Ranges also use `operator|` for view composition:

```cpp
#include <ranges>
using namespace std::views;

// This works in C++20
auto view = vec | filter(pred) | transform(func);

// But with fat_p::operator| in scope...
using namespace fat_p;

// AMBIGUITY! Both fat_p and ranges define operator|
auto view = vec | filter(pred);  // Compilation error!
```

### Using pipe() Wrapper

The `pipe()` wrapper explicitly marks values for fat_p piping:

```cpp
#include <ranges>
#include "PipeOperator.h"

using namespace std::views;

// Use ranges normally
auto view = vec | filter(pred) | transform(func);

// Use fat_p pipe explicitly
auto result = fat_p::pipe(my_expected) | validate | process;
```

### Scoped using Declarations

Limit the scope of `using` declarations:

```cpp
Expected<int> process(Expected<int> input) {
    using fat_p::operator|;  // Only in this function!
    
    return input
        | validate
        | transform
        | finalize;
}

// Outside, ranges work normally
void use_ranges(std::vector<int>& vec) {
    using namespace std::views;
    auto filtered = vec | filter([](int x) { return x > 0; });
}
```

---

## Performance Characteristics

### Zero-Overhead Abstraction

The pipe operator compiles to direct function calls with no runtime overhead:

```cpp
// This:
auto result = x | f | g | h;

// Compiles to essentially:
auto result = h(g(f(x)));
```

No virtual calls, no heap allocations, no indirection.

### Benchmark Results

Measured on typical hardware (Intel Core i7, GCC 13, -O2):

| Operation | Time | Notes |
|-----------|------|-------|
| Basic pipe (`int \| func`) | <1 ns | Optimized away |
| Expected map | ~2 ns | Branch prediction |
| Expected bind | ~3 ns | Additional check |
| 5-stage pipeline | ~10 ns | Linear scaling |
| Error propagation | ~5 ns | Early exit |

### Compiler Optimization

Modern compilers aggressively inline and optimize pipe chains:

```cpp
// Before optimization (conceptual)
auto r1 = f(x);
auto r2 = g(r1);
auto r3 = h(r2);
return r3;

// After optimization (actual)
return h_g_f_combined(x);  // Functions inlined
```

Assembly inspection shows zero overhead compared to hand-written code.

---

## Comparison with Alternatives

### PipeOperator vs Method Chaining

```cpp
// Method chaining (requires method on type)
auto result = value.transform(f).and_then(g).map(h);

// Pipe operator (works with free functions)
auto result = value | f | g | h;
```

**PipeOperator advantages:**
- Works with free functions and lambdas
- No modification to existing types
- Consistent syntax regardless of function source

**Method chaining advantages:**
- No ADL/namespace issues
- Works everywhere without imports
- IDE autocomplete support

### PipeOperator vs Ranges

| Feature | PipeOperator | Ranges |
|---------|-------------|--------|
| Primary use | Value transformation | View composition |
| Evaluation | Eager | Lazy |
| Error handling | Built-in (Expected) | None (exceptions) |
| C++ version | C++17 | C++20 |
| Conflict | Possible | Possible |

Use **PipeOperator** for: Error-handling pipelines, immediate computation
Use **Ranges** for: Lazy sequences, algorithm composition

### PipeOperator vs Rust

| Feature | fat_p Pipe | Rust `?` |
|---------|-----------|----------|
| Syntax | `exp \| func` | `exp?` |
| Error type | Must match | Via From trait |
| Propagation | Automatic | Automatic |
| Early return | No | Yes |
| Language support | Library | Native |

Rust's `?` is slightly more ergonomic for early returns, but fat_p pipe achieves similar expressiveness without language changes.

---

## Advanced Usage

### Custom Expected Types

PipeOperator works with any `ExpectedImpl` instantiation:

```cpp
// Custom storage policy
using FastExpected = ExpectedImpl<int, int, TrivialStorage>;

auto result = FastExpected(42) 
    | [](int x) { return x * 2; };
// Works identically
```

### Stateful Lambdas

Capture state for accumulation or logging:

```cpp
int call_count = 0;

auto counting_transform = [&call_count](int x) {
    call_count++;
    return x * 2;
};

auto result = Expected<int>(5) | counting_transform;
// call_count == 1 if success, 0 if error
```

### Exception-Free Pipelines

Combine with `EXPECTED_TRY` for complex flows:

```cpp
Expected<Result> complex_operation(Input input) {
    // Use EXPECTED_TRY for early returns
    EXPECTED_TRY(validated, validate(input));
    EXPECTED_TRY(processed, process(validated));
    
    // Use pipe for linear chains
    return Expected<Data>(processed)
        | transform
        | finalize
        | package;
}
```

---

## Integration with fat_p Components

PipeOperator integrates naturally with other fat_p components:

```cpp
// With Expected and SmallVector
SmallVector<Expected<int>, 4> results;
for (auto& input : inputs) {
    results.push_back(input | parse | validate);
}

// With Signal (process signal results)
signal.emitCollect() | aggregate | analyze;

// With StrongId (type-safe transformations)
UserId id = ...;
auto result = Expected<UserId>(id)
    | [](UserId id) { return lookup(id); }
    | [](User u) { return u.permissions(); };
```

---

## Troubleshooting

### Common Issues

**Issue: Pipeline compiles but doesn't behave as expected**

Symptom: Functions seem to be called in wrong order or not at all.

Cause: Operator precedence. `|` has lower precedence than most operators.

```cpp
// WRONG - parsed as (a | b) + c
auto result = a | b + c;

// RIGHT - use parentheses
auto result = a | (b + c);

// Or break into steps
auto temp = b + c;
auto result = a | temp;
```

**Issue: Expected<void> doesn't compile with pipe**

Symptom: `*exp` causes compilation error.

Cause: Using wrong overload (void Expected has no value to dereference).

Solution: Ensure you're including the updated PipeOperator.h with void specializations.

### Compilation Errors

**Error: `ambiguous overload for 'operator|'`**

Cause: Conflict with C++20 Ranges or another library.

```cpp
// Solution 1: Use explicit wrapper
auto result = fat_p::pipe(value) | func;

// Solution 2: Scoped using
{
    using fat_p::operator|;
    auto result = value | func;
}
```

**Error: `no matching function for call to 'operator|'`**

Cause: Function signature doesn't match Expected value type.

```cpp
Expected<int> exp(42);
auto wrong = exp | [](std::string s) { return s.length(); };
// Error: can't call string function with int

auto right = exp | [](int x) { return x * 2; };  // OK
```

**Error: `is_expected_v` always false**

Cause: Missing specialization or wrong namespace.

Solution: Ensure Expected.h is included before PipeOperator.h and that `is_expected` specialization exists.

### Runtime Issues

**Issue: Error lost in pipeline**

Symptom: Error message is empty or generic.

Cause: Error type mismatch causes conversion.

```cpp
Expected<int, std::string> exp(unexpected("detailed error"));
auto result = exp | [](int x) -> Expected<int, int> {
    return x;  // Error type changed!
};
// result.error() may not contain original message
```

**Issue: "T and E must be distinct types" compilation error**

Symptom: Static assertion failure when T equals E.

Cause: fat_p::Expected requires distinct value and error types.

```cpp
// ERROR: T and E are both std::string
Expected<std::string, std::string> exp("hello");

// Solution: Use a distinct error type
enum class ParseError { InvalidFormat, TooLong };
Expected<std::string, ParseError> exp("hello");

// Or use a wrapper type
struct ErrorMsg { std::string msg; };
Expected<std::string, ErrorMsg> exp("hello");
```

**Issue: Side effects executed despite error**

Symptom: Logging or state changes occur even when pipeline has error.

Cause: Side effects in const& overload with error Expected.

```cpp
const Expected<int> exp(unexpected("error"));
int count = 0;

auto func = [&count](int x) { 
    count++;  // Side effect
    return x; 
};

auto result = exp | func;
// count is still 0 (func never called)
```

---

## Best Practices

1. **Keep pipelines readable**
   ```cpp
   // GOOD - clear stages
   auto result = input
       | parse
       | validate
       | transform
       | format;
   
   // BAD - too dense
   auto result = input | parse | validate | transform | format | another | yet_another;
   ```

2. **Name lambdas for clarity**
   ```cpp
   // GOOD - self-documenting
   auto parse_json = [](std::string s) { ... };
   auto validate_schema = [](Json j) { ... };
   
   auto result = input | parse_json | validate_schema;
   
   // BAD - anonymous soup
   auto result = input | [](auto s) { ... } | [](auto j) { ... };
   ```

3. **Use EXPECTED_TRY for complex control flow**
   ```cpp
   // Pipe for linear flow
   auto simple = a | b | c;
   
   // EXPECTED_TRY for branching
   EXPECTED_TRY(x, compute());
   if (x > threshold) {
       EXPECTED_TRY(y, special_case(x));
       return y;
   }
   return normal_case(x);
   ```

4. **Avoid side effects in middle stages**
   ```cpp
   // GOOD - pure functions in middle
   auto result = input
       | parse        // pure
       | validate     // pure
       | transform    // pure
       | [&](...) { log(...); return ...; };  // side effect at end
   
   // BAD - side effects scattered
   auto result = input
       | [&](auto x) { log(x); return parse(x); }   // logging mixed in
       | [&](auto x) { metrics++; return x; }       // metrics mixed in
       | transform;
   ```

5. **Match error types**
   ```cpp
   // GOOD - consistent error type
   auto a = [](int x) -> Expected<int, std::string> { ... };
   auto b = [](int x) -> Expected<int, std::string> { ... };
   
   // BAD - error type mismatch
   auto a = [](int x) -> Expected<int, std::string> { ... };
   auto b = [](int x) -> Expected<int, int> { ... };  // Different!
   ```

---

## Design Decisions and Tradeoffs

### Why Operator Overload Instead of Named Function?

**Decision:** Use `operator|` instead of `pipe(a, f)`.

**Tradeoff:** Potential conflicts with C++20 Ranges.

**Rationale:**
- Natural left-to-right reading order
- Matches F#, Elixir, and shell conventions
- Chainable without nesting: `a | b | c` vs `pipe(pipe(a, b), c)`
- Visually distinct pipeline stages

### Why Automatic Map/Bind Detection?

**Decision:** SFINAE selects map vs bind based on return type.

**Tradeoff:** Magic behavior may surprise users.

**Rationale:**
- Eliminates manual `.map()` vs `.and_then()` choice
- Prevents double-wrapping errors
- Matches Rust's behavior
- Reduces boilerplate

### Why No Lazy Evaluation?

**Decision:** Pipe evaluates eagerly.

**Tradeoff:** Can't represent infinite sequences.

**Rationale:**
- Simpler mental model
- Better error locality (fails at source)
- C++20 Ranges handles lazy case well
- Expected semantics require immediate evaluation for error detection

### Why Support Void Expected?

**Decision:** Full void specialization support.

**Tradeoff:** More complex implementation (8 overloads vs 4).

**Rationale:**
- Status-only operations are common
- `initialize() | configure() | start()` pattern
- Symmetry with non-void Expected
- Rust's `Result<(), E>` equivalent

---

## Summary

### Key Features

- **Zero-overhead** function composition via `operator|`
- **Automatic map/bind** selection based on return type
- **Full Expected\<void\>** support for status pipelines
- **Error propagation** - first error short-circuits
- **Type preservation** - storage policy flows through
- **C++17 compatible** - works without concepts/ranges

### Pipe Semantics Quick Reference

| Input | Function Returns | Result | Operation |
|-------|-----------------|--------|-----------|
| `T` | `U` | `U` | Direct call |
| `Expected<T>` | `U` | `Expected<U>` | Map |
| `Expected<T>` | `Expected<U>` | `Expected<U>` | Bind |
| `Expected<void>` | `U` | `Expected<U>` | Void map |
| `Expected<void>` | `Expected<U>` | `Expected<U>` | Void bind |

### Quick Start

```cpp
#include "Expected.h"
#include "PipeOperator.h"

int main() {
    using namespace fat_p;
    
    // Value pipeline
    int x = 5 | [](int n) { return n * 2; } | [](int n) { return n + 1; };
    // x == 11
    
    // Expected pipeline
    auto result = Expected<int>(42)
        | [](int x) -> Expected<int> { 
            if (x < 0) return unexpected("negative");
            return x * 2;
        }
        | [](int x) { return std::to_string(x); };
    // result: Expected<std::string>("84")
    
    return 0;
}
```

### Related Components

- `Expected.h` - Result type with error handling
- `FatPTypeTraits.h` - Type detection traits
- `EXPECTED_TRY` - Early-return error propagation
