# Enforce System User Manual

## Table of Contents

1. [What is Enforce?](#what-is-enforce)
   - [The Contract Enforcement Problem](#the-contract-enforcement-problem)
   - [Why Contracts Matter](#why-contracts-matter)
   - [The C++ Landscape](#the-c-landscape)
   - [Where Enforce Fits](#where-enforce-fits)
2. [Core Architecture](#core-architecture)
   - [Design Principles](#design-principles)
   - [Component Diagram](#component-diagram)
   - [Policy-Based Design](#policy-based-design)
3. [Getting Started](#getting-started)
   - [Prerequisites](#prerequisites)
   - [Integration](#integration)
   - [First Program](#first-program)
4. [Core Macros](#core-macros)
   - [Condition Macros](#condition-macros)
   - [Expected Macros](#expected-macros)
   - [Predicate Macros](#predicate-macros)
5. [Policies](#policies)
   - [DebugOnlyPolicy](#debugonlypolicy)
   - [AlwaysEnforcePolicy](#alwaysenforcepolicy)
   - [WarningPolicy](#warningpolicy)
   - [NoThrowPolicy](#nothrowpolicy)
   - [AbortPolicy](#abortpolicy)
6. [Predicates](#predicates)
   - [Core Predicates](#core-predicates)
   - [Container Predicates](#container-predicates)
   - [Range Predicates](#range-predicates)
   - [Floating-Point Predicates](#floating-point-predicates)
   - [Iterator Predicates](#iterator-predicates)
7. [Error Messages](#error-messages)
   - [Message Formatting](#message-formatting)
   - [Full Error Format](#full-error-format)
   - [Custom Stringification](#custom-stringification)
8. [Expected Integration](#expected-integration)
   - [Basic Pattern](#basic-pattern)
   - [Error Propagation](#error-propagation)
9. [Contextual Enforcement](#contextual-enforcement)
   - [Why Contextual Enforcement Matters](#why-contextual-enforcement-matters)
   - [The std::terminate Trap](#the-stdterminate-trap)
   - [Standard Library Optimization Implications](#standard-library-optimization-implications)
   - [Using Contextual Enforcement](#using-contextual-enforcement)
   - [Contextual Macro Reference](#contextual-macro-reference)
10. [Custom Policies](#custom-policies)
    - [Custom Predicates](#custom-predicates)
    - [Custom Raisers](#custom-raisers)
    - [Custom Violation Handler](#custom-violation-handler)
11. [Performance Characteristics](#performance-characteristics)
    - [Benchmark Methodology](#benchmark-methodology)
    - [Benchmark Results](#benchmark-results)
    - [Optimization Guidelines](#optimization-guidelines)
12. [Comparison with Other Approaches](#comparison-with-other-approaches)
    - [Feature Comparison](#feature-comparison)
    - [Code Comparison](#code-comparison)
13. [Migration Guide](#migration-guide)
    - [From assert()](#from-assert)
    - [From Manual if-throw](#from-manual-if-throw)
14. [Best Practices](#best-practices)
15. [Troubleshooting](#troubleshooting)
    - [Compilation Errors](#compilation-errors)
    - [Runtime Issues](#runtime-issues)
16. [Summary](#summary)

---

## What is Enforce?

### The Contract Enforcement Problem

Every function makes promises. A sorting function promises to return elements in order. A memory allocator promises to return valid pointers or indicate failure. A division function promises not to divide by zero. These promises are called *contracts*.

Design by Contract (DbC) formalizes this idea: functions have preconditions (what callers must guarantee), postconditions (what the function guarantees in return), and invariants (what remains true throughout). When contracts are violated, bugs have occurred, and the program is in an undefined state.

The challenge is not understanding contracts; it is enforcing them consistently in C++:

```cpp
// Manual contract enforcement: verbose and inconsistent
void process_buffer(const char* data, size_t size, char* output)
{
    if (data == nullptr)
    {
        throw std::invalid_argument("data is null");
    }
    if (output == nullptr)
    {
        throw std::invalid_argument("output is null");
    }
    if (size == 0)
    {
        throw std::invalid_argument("size is zero");
    }
    if (size > MAX_BUFFER_SIZE)
    {
        throw std::length_error("size exceeds maximum");
    }
    
    // The actual logic is buried under boilerplate
    // ...
}
```

This approach has several problems:

| Problem | Impact |
|---------|--------|
| Boilerplate obscures logic | Real code is 4 lines; checking is 16 lines |
| Inconsistent exception types | Some throw `invalid_argument`, others `length_error` |
| No way to disable in release | Every check runs in production, even internal invariants |
| No structured diagnostics | Where did it fail? What was the actual value? |
| Copy-paste errors | Each check is handwritten, introducing variation |

Using `assert()` trades one set of problems for another:

```cpp
void process_buffer(const char* data, size_t size, char* output)
{
    assert(data != nullptr);
    assert(output != nullptr);
    assert(size > 0);
    assert(size <= MAX_BUFFER_SIZE);
    
    // ...
}
```

| Problem | Impact |
|---------|--------|
| Disappears in release | `assert()` compiles to nothing with `NDEBUG` |
| Terminates program | No recovery possible; `std::abort()` is called |
| No exceptions | Cannot catch and handle in calling code |
| Minimal diagnostics | Just the expression and file/line |
| All-or-nothing | Cannot keep some checks while removing others |

### Why Contracts Matter

Contract violations are not ordinary errors. They represent bugs in the program, not runtime conditions like "file not found" or "network timeout." This distinction matters for how we handle them.

**Expected errors** should use normal error handling:

```cpp
// File might not exist - this is expected
std::optional<std::string> read_config(const std::string& path)
{
    std::ifstream file(path);
    if (!file)
    {
        return std::nullopt;  // Normal error path
    }
    // ...
}
```

**Contract violations** indicate programmer error:

```cpp
// Caller promised data is valid - violation means a bug
void process(const Data& data)
{
    // If data.is_valid() is false, the caller has a bug.
    // This is not a runtime condition; it is a programming error.
    always_enforce(data.is_valid(), "Invalid data passed to process()");
    // ...
}
```

When contracts are violated, the program is already in an undefined state. The goal of contract enforcement is not graceful recovery but rapid detection. Bugs caught at the point of contract violation are far easier to diagnose than bugs that manifest later as corrupted data or mysterious crashes.

This is why the Enforce system provides both throwing and non-throwing options. Sometimes you want to throw an exception to unwind the stack and report the bug. Sometimes you are in a destructor or `noexcept` function where throwing would terminate the program. The appropriate response depends on context.

### The C++ Landscape

Several solutions exist for contract enforcement in C++:

| Solution | Pros | Cons |
|----------|------|------|
| `assert()` | Simple, standard, zero overhead in release | All-or-nothing, terminates program, no exceptions |
| Manual if-throw | Full control over behavior | Verbose, inconsistent, error-prone |
| C++20 Contracts | Standard, compiler-integrated | Limited compiler support, inflexible |
| Boost.Contract | Comprehensive, well-tested | Heavy dependency, complex API, runtime overhead |
| GSL Expects/Ensures | Simple, well-designed | Limited to terminate behavior, no flexibility |
| Custom macros | Tailored to project needs | Maintenance burden, often poorly designed |

C++20 introduced language-level contracts, but adoption is limited. As of 2024, no major compiler fully implements them, and the specification continues to evolve. The Enforce system provides similar functionality today, using C++17 features.

### Where Enforce Fits

The Enforce system occupies a specific niche: **policy-based contract enforcement with zero-overhead debug checks**. It provides:

- **Policy-based error handling**: Choose throw, warn, abort, log, or return Expected per-check
- **Rich predicates**: Type-safe, reusable condition checking with semantic names
- **Detailed diagnostics**: File, line, expression, and custom messages automatically
- **Zero overhead option**: Debug-only macros compile to nothing in release
- **Contextual awareness**: Automatic adaptation to noexcept functions
- **Thread-safe**: All operations are thread-safe by default
- **Header-only**: No library to link, no dependencies beyond C++17 standard library

**When to use Enforce:**

- Function precondition and postcondition checking
- Internal invariant validation during development
- API boundary protection in library code
- Debug-only expensive validations (deep structure checks)
- Exception-free error reporting (via Expected integration)
- Destructor and noexcept function safety

**When NOT to use Enforce:**

- User input validation (use proper error handling with user-friendly messages)
- Expected failure cases like file-not-found (use explicit error handling)
- Performance-critical inner loops (even debug checks add some overhead)
- Recoverable runtime conditions (use exceptions or error codes directly)

---

## Core Architecture

### Design Principles

The Enforce system is built on three key principles:

**1. Separation of Concerns**

The check logic, failure behavior, and message formatting are independent components. This separation allows changing the failure behavior without modifying the check, and adding new predicates without touching the enforcement machinery.

**2. Compile-Time Policy Selection**

The failure behavior is determined entirely at compile time through template policies. This means the compiler can optimize away unused code paths entirely. A debug-only check in a release build does not generate a single instruction.

**3. Zero-Overhead Abstraction**

When a policy specifies no action (like `DebugOnlyPolicy` in release builds), the entire enforcement call compiles to nothing. Not a function call that returns immediately, but literally zero instructions. This is verified through disassembly.

### Component Diagram

```mermaid
flowchart TB
    subgraph UserCode["User Code"]
        UC["enforce(ptr != nullptr, 'pointer is null')"]
    end
    
    subgraph Macros["enforce.h Macros"]
        IMPL["enforce_policy_impl&lt;Policy&gt;(condition, expression, locus)"]
    end
    
    UserCode --> Macros
    
    Macros --> RS["RaiserSelector<br/>Policy to Raiser mapping<br/>DebugOnly → NoOp<br/>Always → Logic<br/>Warning → Cerr"]
    Macros --> ENF["Enforcer<br/>RAII object that<br/>calls Raiser on<br/>destruction if<br/>condition failed"]
    Macros --> MB["MessageBuilder<br/>Builds formatted<br/>error message from<br/>variadic args"]
    
    ENF --> Raisers
    
    subgraph Raisers["Raisers"]
        LR["LogicRaiser<br/>throws<br/>LogicContractError"]
        AR["AbortRaiser<br/>aborts<br/>program"]
        WR["WarningRaiser<br/>logs to<br/>stderr"]
        NTR["NoThrowRaiser<br/>calls<br/>handler"]
        NOR["NoOpRaiser<br/>nothing<br/>zero overhead"]
    end
```

### Policy-Based Design

The central insight is that the *consequence* of a contract violation should be decoupled from the *check* itself. The same condition might warrant different responses in different contexts:

```cpp
// The same logical check with different consequences:

enforce(ptr != nullptr, "...");        // Debug: throw, Release: nothing
always_enforce(ptr != nullptr, "..."); // Always throw
enforce_warn(ptr != nullptr, "...");   // Log and continue
noexcept_enforce(ptr != nullptr, "...");// Call handler, never throw
abort_enforce(ptr != nullptr, "...");  // Immediate termination
```

This is implemented through the `RaiserSelector` template, which maps policies to raisers at compile time:

```cpp
template <typename Policy>
struct RaiserSelector;

template <>
struct RaiserSelector<DebugOnlyPolicy>
{
#ifdef NDEBUG
    using type = NoOpRaiser;    // Zero overhead in release
#else
    using type = LogicRaiser;   // Throws in debug
#endif
};

template <>
struct RaiserSelector<AlwaysEnforcePolicy>
{
    using type = LogicRaiser;   // Always throws
};

template <>
struct RaiserSelector<NoThrowPolicy>
{
    using type = NoThrowRaiser; // Calls handler, never throws
};
```

The compiler sees the policy at compile time, selects the appropriate raiser, and (in the case of `NoOpRaiser`) optimizes away the entire enforcement call.

---

## Getting Started

### Prerequisites

| Requirement | Minimum Version |
|-------------|-----------------|
| C++ Standard | C++17 |
| Compiler | GCC 7+, Clang 5+, MSVC 2017+ |
| Dependencies | None (header-only) |

### Integration

The Enforce system consists of these headers:

| Header | Purpose |
|--------|---------|
| `enforce.h` | Main macros - include this for most use cases |
| `enforce_predicates.h` | Predicate definitions (included by enforce.h) |
| `enforce_raisers.h` | Raiser policies (included by enforce.h) |
| `enforce_raiser_selector.h` | Policy-to-raiser mapping (included by enforce.h) |
| `enforce_enforcers.h` | RAII enforcer class (included by enforce.h) |
| `enforce_contextual.h` | Contextual/noexcept-aware enforcement |
| `ContractException.h` | Exception class hierarchy |

Basic usage requires only:

```cpp
#include "enforce.h"
```

For contextual enforcement (noexcept-aware):

```cpp
#include "enforce.h"
#include "enforce_contextual.h"
```

### First Program

```cpp
#include "enforce.h"
#include <iostream>
#include <vector>

// A function with enforced preconditions
double safe_divide(double numerator, double denominator)
{
    // Precondition: denominator must not be zero
    always_enforce(denominator != 0.0, "Division by zero: denominator=", denominator);
    
    return numerator / denominator;
}

// A function with debug-only checks
void process_data(const std::vector<int>& data, size_t index)
{
    // Debug-only bounds check (zero overhead in release)
    enforce(index < data.size(), 
            "Index out of bounds: index=", index, " size=", data.size());
    
    // Use the data...
    std::cout << "Value at index " << index << ": " << data[index] << "\n";
}

int main()
{
    try
    {
        std::cout << "10 / 2 = " << safe_divide(10, 2) << "\n";
        
        std::vector<int> vec = {1, 2, 3, 4, 5};
        process_data(vec, 2);
        
        // This will throw:
        std::cout << "10 / 0 = " << safe_divide(10, 0) << "\n";
    }
    catch (const fat_p::LogicContractError& e)
    {
        std::cerr << "Contract violation: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}
```

Compile and run:

```bash
# Debug build (checks enabled)
g++ -std=c++17 -g example.cpp -o example

# Release build (debug checks disabled)
g++ -std=c++17 -O2 -DNDEBUG example.cpp -o example_release
```

---

## Core Macros

### Condition Macros

| Macro | Active In | On Failure |
|-------|-----------|------------|
| `enforce(cond, ...)` | Debug only | Throws `LogicContractError` |
| `always_enforce(cond, ...)` | Always | Throws `LogicContractError` |
| `enforce_warn(cond, ...)` | Always | Logs to stderr, continues |
| `noexcept_enforce(cond, ...)` | Always | Calls violation handler, no throw |
| `abort_enforce(cond, ...)` | Always | Calls `std::abort()` |

```cpp
void example(int* ptr, int count)
{
    // Debug-only check - zero overhead in release
    enforce(ptr != nullptr, "Null pointer");
    
    // Always checked - for public API boundaries
    always_enforce(count > 0, "Count must be positive, got: ", count);
    
    // Warning only - log and continue
    enforce_warn(count < 1000, "Large count may be slow: ", count);
    
    // For noexcept functions and destructors
    noexcept_enforce(is_valid_state(), "Invalid state");
    
    // For unrecoverable errors
    abort_enforce(googole_connection_ok(), "Lost connection");
}
```

### Expected Macros

These macros return `Expected<void, std::string>` instead of throwing:

| Macro | Returns |
|-------|---------|
| `enforce_expected(cond, ...)` | `Expected<void, std::string>` |
| `always_enforce_expected(cond, ...)` | `Expected<void, std::string>` |
| `enforce_predicate_expected(Pred, target, ...)` | `Expected<bool, std::string>` |

```cpp
fat_p::Expected<int, std::string> parse_positive(const std::string& s)
{
    auto check = enforce_expected(!s.empty(), "Empty input string");
    if (!check)
    {
        return fat_p::unexpected(check.error());
    }
    
    int value = std::stoi(s);
    
    check = enforce_expected(value > 0, "Value must be positive, got: ", value);
    if (!check)
    {
        return fat_p::unexpected(check.error());
    }
    
    return value;
}
```

### Predicate Macros

Generic predicate macros work with any predicate type:

```cpp
// Single argument predicates
always_enforce_1(NotNullPredicate, ptr, "Pointer is null");
enforce_1(IsPositivePredicate, count, "Count must be positive");

// Two argument predicates
always_enforce_2(ValidIndexPredicate, idx, container, "Index out of bounds");
always_enforce_2(HasSizePredicate, expected, container, "Wrong size");

// Three argument predicates
always_enforce_3(InRangePredicate, value, min, max, "Value out of range");
always_enforce_3(ApproxEqualPredicate, epsilon, a, b, "Values not equal");
```

Convenience macros for common predicates:

```cpp
// Null checks
always_enforce_not_null(ptr, "Null pointer");
debug_enforce_not_null(ptr, "Debug null check");

// Numeric checks
always_enforce_is_positive(count, "Must be positive");
always_enforce_is_non_negative(offset, "Must be non-negative");
always_enforce_in_range(0, 100, value, "Value out of range");

// Container checks
always_enforce_not_empty(container, "Container is empty");
always_enforce_is_sorted(container, "Container must be sorted");
always_enforce_valid_index(idx, container, "Invalid index");

// Floating-point checks
always_enforce_is_finite(result, "Result is NaN or Inf");
always_enforce_approx_equal(0.001, expected, actual, "Values differ");
```

---

## Policies

### DebugOnlyPolicy

Active only in debug builds (when `NDEBUG` is not defined). Compiles to zero instructions in release builds.

```cpp
// Only checked in debug builds
enforce(index < size, "Index out of bounds");
```

Use for internal invariants that should never fail if the code is correct, but are too expensive to check in production.

### AlwaysEnforcePolicy

Active in all builds. Throws `LogicContractError` on failure.

```cpp
// Always checked, even in release
always_enforce(user_input > 0, "Invalid input: ", user_input);
```

Use for public API preconditions where callers might pass invalid arguments.

### WarningPolicy

Logs the violation to stderr but continues execution.

```cpp
// Logs warning but continues
enforce_warn(cache.size() < 10000, "Cache growing large: ", cache.size());
```

Use for non-critical conditions where you want visibility without stopping execution.

### NoThrowPolicy

Calls the global violation handler but never throws. Safe for `noexcept` functions.

```cpp
~Resource() noexcept
{
    noexcept_enforce(handle_ != INVALID_HANDLE, "Invalid handle in destructor");
    release(handle_);
}
```

Use in:
- Destructors
- `noexcept` functions
- Signal handlers
- Any context where exceptions cannot propagate

### AbortPolicy

Immediately terminates the program with `std::abort()`.

```cpp
// No recovery possible - terminate immediately
abort_enforce(googole_connection_ok(), "Critical: lost connection");
```

Use for truly unrecoverable situations where continuing would cause data corruption or security vulnerabilities.

---

## Predicates

Predicates are reusable condition checks that provide semantic meaning and type safety. They are struct types with a static `check()` method.

### Core Predicates

| Predicate | Check | Common Use |
|-----------|-------|------------|
| `NotNullPredicate` | `ptr != nullptr` | Pointer validation |
| `IsNullPredicate` | `ptr == nullptr` | Ensuring cleanup |
| `IsPositivePredicate` | `value > 0` | Counts, sizes |
| `IsNegativePredicate` | `value < 0` | Error codes |
| `IsNonNegativePredicate` | `value >= 0` | Offsets, indices |
| `IsNonPositivePredicate` | `value <= 0` | Upper bounds |
| `IsZeroPredicate` | `value == 0` | Initialization |
| `IsNonZeroPredicate` | `value != 0` | Divisors |
| `IsTruePredicate` | `value == true` | Boolean flags |
| `IsFalsePredicate` | `value == false` | Boolean flags |

### Container Predicates

| Predicate | Check | Arguments |
|-----------|-------|-----------|
| `NotEmptyPredicate` | `!container.empty()` | container |
| `IsEmptyPredicate` | `container.empty()` | container |
| `HasSizePredicate` | `container.size() == expected` | expected, container |
| `IsSortedPredicate` | `std::is_sorted(...)` | container |
| `ContainerIsUniquePredicate` | All elements unique | container |
| `ValidIndexPredicate` | `idx < container.size()` | idx, container |

### Range Predicates

| Predicate | Check | Arguments |
|-----------|-------|-----------|
| `InRangePredicate` | `min <= value <= max` | value, min, max |
| `InRangeExclusivePredicate` | `min < value < max` | value, min, max |
| `GreaterThanPredicate` | `value > threshold` | value, threshold |
| `LessThanPredicate` | `value < threshold` | value, threshold |
| `GreaterOrEqualPredicate` | `value >= threshold` | value, threshold |
| `LessOrEqualPredicate` | `value <= threshold` | value, threshold |

### Floating-Point Predicates

| Predicate | Check | Arguments |
|-----------|-------|-----------|
| `IsFinitePredicate` | `std::isfinite(value)` | value |
| `IsNaNPredicate` | `std::isnan(value)` | value |
| `IsInfPredicate` | `std::isinf(value)` | value |
| `IsNormalPredicate` | `std::isnormal(value)` | value |
| `ApproxEqualPredicate` | `|a - b| <= epsilon` | epsilon, a, b |

### Iterator Predicates

| Predicate | Check | Arguments |
|-----------|-------|-----------|
| `ValidIteratorPredicate` | Iterator dereferenceable | iter, end |
| `IteratorInRangePredicate` | Iterator in [begin, end) | iter, begin, end |

---

## Error Messages

### Message Formatting

Error messages support variadic arguments that are automatically converted to strings:

```cpp
always_enforce(count > 0, "Count must be positive, got: ", count);
// Output: "Count must be positive, got: -5"

always_enforce(index < size, 
               "Index ", index, " out of bounds for size ", size);
// Output: "Index 42 out of bounds for size 10"
```

### Full Error Format

When a contract is violated, the full error message includes:

```
Exception: 
    Condition: index < size
    Locus: myfile.cpp:123
    Message: Index 42 out of bounds for size 10
```

### Custom Stringification

Any type with `operator<<` works automatically. For custom types:

```cpp
struct Point
{
    int x, y;
    
    friend std::ostream& operator<<(std::ostream& os, const Point& p)
    {
        return os << "(" << p.x << ", " << p.y << ")";
    }
};

Point p{10, 20};
always_enforce(p.x >= 0, "Invalid point: ", p);
// Message: "Invalid point: (10, 20)"
```

---

## Expected Integration

### Basic Pattern

The Expected macros return `Expected<void, std::string>` instead of throwing, enabling monadic error handling:

```cpp
fat_p::Expected<void, std::string> validate_config(const Config& cfg)
{
    auto check = enforce_expected(cfg.port > 0, "Invalid port: ", cfg.port);
    if (!check)
    {
        return fat_p::unexpected(check.error());
    }
    
    check = enforce_expected(cfg.max_connections > 0, "Invalid max_connections");
    if (!check)
    {
        return fat_p::unexpected(check.error());
    }
    
    return {};  // Success
}
```

### Error Propagation

Errors can be propagated up the call stack without exceptions:

```cpp
fat_p::Expected<Connection, std::string> connect(const Config& cfg)
{
    auto validation = validate_config(cfg);
    if (!validation)
    {
        return fat_p::unexpected(validation.error());
    }
    
    // Proceed with connection...
}
```

---

## Contextual Enforcement

### Why Contextual Enforcement Matters

The contextual enforcement system addresses a subtle but critical problem in C++: **exceptions escaping `noexcept` functions cause immediate program termination**.

This problem is more pervasive than it might appear. Consider a simple getter in a class:

```cpp
class Resource
{
    Handle handle_;
    
public:
    Handle get_handle() const noexcept
    {
        // What if handle_ is invalid?
        // We cannot throw - this function is noexcept
        return handle_;
    }
};
```

The `noexcept` specifier is not just documentation; it is a contract with the compiler and the Standard Library. When a function is marked `noexcept`, the compiler generates code assuming no exception will propagate. If one does, `std::terminate()` is called immediately.

### The std::terminate Trap

```cpp
void dangerous_function() noexcept
{
    // This looks innocent...
    always_enforce(some_condition(), "Condition failed");
    
    // But if some_condition() returns false, always_enforce throws,
    // and since we are in a noexcept function, the program calls
    // std::terminate() - an immediate, unrecoverable crash with
    // no stack unwinding and no cleanup.
}
```

This is a common source of production crashes. The code works in testing where conditions are met, then fails catastrophically in production when an edge case triggers the violation.

The contextual enforcement system solves this by automatically detecting the `noexcept` specification of the enclosing function and selecting an appropriate raiser:

```cpp
void safe_function() noexcept
{
    // Contextual enforcement detects noexcept and uses NoThrowRaiser
    contextual_enforce(&safe_function, some_condition(), "Condition failed");
    
    // If condition fails:
    // 1. The violation is logged
    // 2. The global violation handler is called
    // 3. Execution continues (or handler calls abort)
    // 4. No exception is thrown
    // 5. No std::terminate
}
```

### Standard Library Optimization Implications

The `noexcept` specifier has significant performance implications beyond exception handling. The Standard Library uses it to optimize container operations:

**Move Operations in Containers**

When `std::vector` needs to reallocate (e.g., during `push_back`), it must move or copy existing elements to the new storage. If the element's move constructor is `noexcept`, the vector can safely move elements. If the move constructor might throw, the vector must copy elements to maintain the Strong Exception Guarantee.

```cpp
class Widget
{
public:
    // If this can throw, std::vector will copy instead of move
    Widget(Widget&& other);
    
    // If this is noexcept, std::vector will move (faster)
    Widget(Widget&& other) noexcept;
};
```

When your class has invariants that you want to enforce in the move constructor, you must use non-throwing enforcement to keep the `noexcept` guarantee:

```cpp
Widget::Widget(Widget&& other) noexcept
    : data_(std::exchange(other.data_, nullptr))
{
    // Cannot use always_enforce - it throws
    // Must use noexcept_enforce or contextual_enforce
    noexcept_enforce(data_ != nullptr, "Moved-from widget was empty");
}
```

**Cross-Translation-Unit Opacity**

Compilers cannot see across translation unit boundaries without Link-Time Optimization (LTO). This means the compiler cannot infer that a function called from a `noexcept` function will not throw:

```cpp
// In widget.h
class Widget
{
public:
    void validate() const;  // Might this throw? Compiler cannot know
};

// In widget.cpp
void Widget::validate() const
{
    always_enforce(is_valid(), "Invalid widget");  // This throws!
}

// In user.cpp
void process(const Widget& w) noexcept
{
    w.validate();  // Compiler cannot see that this might throw
    // If validate() throws, std::terminate() is called
}
```

The contextual enforcement system makes the intent explicit and ensures safety:

```cpp
void process(const Widget& w) noexcept
{
    // Explicitly non-throwing validation
    contextual_enforce(&process, w.is_valid(), "Invalid widget");
}
```

### Using Contextual Enforcement

The contextual macros require a function pointer as the first argument. This pointer is used to detect the `noexcept` specification at compile time:

```cpp
void throwing_function(int* ptr)
{
    // In a throwing function, uses LogicRaiser (throws on failure)
    contextual_enforce(&throwing_function, ptr != nullptr, "Null pointer");
}

void noexcept_function(int* ptr) noexcept
{
    // In a noexcept function, uses NoThrowRaiser (calls handler, never throws)
    contextual_enforce(&noexcept_function, ptr != nullptr, "Null pointer");
}
```

The raiser selection happens at compile time based on the function pointer's type:

| Function Type | Detected Via | Raiser Selected | Behavior |
|---------------|--------------|-----------------|----------|
| Regular (may throw) | `!noexcept(func(...))` | LogicRaiser | Throws exception |
| `noexcept` | `noexcept(func(...))` | NoThrowRaiser | Calls handler, no throw |

### Contextual Macro Reference

**Basic Contextual Macros:**

```cpp
contextual_enforce(func_ptr, condition, msg...)
contextual_abort(func_ptr, condition, msg...)
contextual_debug(func_ptr, condition, msg...)
```

**Predicate Variants:**

```cpp
contextual_enforce_1(func_ptr, Predicate, target, msg...)
contextual_enforce_2(func_ptr, Predicate, arg1, arg2, msg...)
contextual_enforce_3(func_ptr, Predicate, arg1, arg2, arg3, msg...)
```

**Convenience Predicates:**

```cpp
contextual_enforce_not_null(func_ptr, ptr, msg...)
contextual_enforce_is_positive(func_ptr, value, msg...)
contextual_enforce_is_non_negative(func_ptr, value, msg...)
contextual_enforce_in_range(func_ptr, value, min, max, msg...)
contextual_enforce_not_empty(func_ptr, container, msg...)
// ... and more
```

**Expected Variants:**

```cpp
contextual_enforce_expected(func_ptr, condition, msg...)
contextual_enforce_expected_1(func_ptr, Predicate, target, msg...)
```

**Utility Assessment:**

| Factor | Assessment |
|--------|------------|
| Performance (HPC) | High - Enables Standard Library optimizations |
| Integrity (Safety) | Critical - Prevents `std::terminate` crashes |
| Clarity | High - Documents exception contract explicitly |
| Simplicity | High - Replaces manual `if constexpr` and `std::is_nothrow` logic |

---

## Custom Policies

### Custom Predicates

Creating a custom predicate is straightforward:

```cpp
struct EvenNumberPredicate
{
    template <typename T>
    static constexpr bool check(T value) noexcept
    {
        return value % 2 == 0;
    }
};

// Usage:
always_enforce_1(EvenNumberPredicate, count, "Count must be even");
```

For predicates with multiple arguments:

```cpp
struct DivisibleByPredicate
{
    template <typename T, typename U>
    static constexpr bool check(T value, U divisor) noexcept
    {
        return divisor != 0 && value % divisor == 0;
    }
};

// Usage:
always_enforce_2(DivisibleByPredicate, value, 3, "Value must be divisible by 3");
```

### Custom Raisers

You can create custom raisers for specialized error handling:

```cpp
class MyApplicationError : public std::runtime_error
{
public:
    explicit MyApplicationError(const std::string& msg)
        : std::runtime_error(msg)
    {
    }
};

struct MyAppRaiser : fat_p::CustomRaiser<MyApplicationError>
{
};

// Usage with enforce_policy:
enforce_policy<MyAppRaiser>(condition, "message");
```

### Custom Violation Handler

The global violation handler is called by `noexcept_enforce` and can be customized:

```cpp
void my_violation_handler(const std::string& message)
{
    // Log to file, send to monitoring system, etc.
    std::cerr << "[VIOLATION] " << message << std::endl;
    
    // Optionally abort
    // std::abort();
}

int main()
{
    fat_p::set_violation_handler(my_violation_handler);
    // ...
}
```

To reset to the default handler:

```cpp
fat_p::reset_violation_handler();
```

---

## Performance Characteristics

### Benchmark Methodology

**Test Environment:**

| Component | Specification |
|-----------|---------------|
| Processor | Intel Core i7-8850H @ 2.60 GHz |
| RAM | 32.0 GB |
| OS | Windows 10 x64 |
| Compiler | MSVC 2022 (Release: `/O2 /DNDEBUG`) |

**Methodology:**
- Each operation measured over 1,000,000 iterations
- Results averaged over multiple runs
- Variance within 5% across runs

### Benchmark Results

| Operation | Time (ns) | Notes |
|-----------|-----------|-------|
| `enforce()` (release) | 0 | Compiled out entirely |
| `enforce()` (debug, passing) | 2-5 | Condition evaluation only |
| `always_enforce()` (passing) | 2-5 | Condition evaluation only |
| `always_enforce()` (failing) | 500-2000 | Exception construction |
| `noexcept_enforce()` (passing) | 2-5 | No handler call if passes |
| `noexcept_enforce()` (failing) | 100-500 | Handler call overhead |
| `enforce_warn()` (passing) | 2-5 | No stderr write if passes |
| `enforce_warn()` (failing) | 5000+ | Stderr I/O dominates |
| Predicate check | 1-3 | Inlined by compiler |

### Optimization Guidelines

1. **Use `enforce()` for internal invariants** - Zero overhead in release
2. **Use `always_enforce()` at API boundaries** - Low overhead for passing conditions
3. **Avoid string operations in conditions** - Build message only on failure
4. **Predicates are inlined** - No function call overhead for simple predicates

```cpp
// Good: message built only on failure
always_enforce(ptr != nullptr, "Null pointer at index ", index);

// Avoid: string concatenation happens unconditionally
std::string msg = "Null pointer at index " + std::to_string(index);
always_enforce(ptr != nullptr, msg);
```

---

## Comparison with Other Approaches

### Feature Comparison

| Feature | Enforce | assert() | GSL | Boost.Contract |
|---------|---------|----------|-----|----------------|
| Debug-only checks | ✓ | ✓ | ✗ | ✓ |
| Always-on checks | ✓ | ✗ | ✓ | ✓ |
| Throws exception | ✓ | ✗ | ✗ | ✓ |
| Non-throwing option | ✓ | ✗ | ✗ | ✗ |
| Expected integration | ✓ | ✗ | ✗ | ✗ |
| Custom predicates | ✓ | ✗ | ✗ | ✓ |
| Detailed diagnostics | ✓ | Limited | Limited | ✓ |
| Header-only | ✓ | ✓ | ✓ | ✗ |
| No dependencies | ✓ | ✓ | ✗ | ✗ |
| Noexcept-aware | ✓ | ✗ | ✗ | ✗ |

### Code Comparison

**With assert():**

```cpp
void process(int* data, size_t size)
{
    assert(data != nullptr);
    assert(size > 0);
    assert(size <= MAX_SIZE);
    // No recovery if assertions fail
    // No assertions in release
}
```

**With Enforce:**

```cpp
void process(int* data, size_t size)
{
    always_enforce_not_null(data, "data parameter");
    always_enforce_is_positive(size, "size parameter");
    always_enforce(size <= MAX_SIZE, "size ", size, " exceeds max ", MAX_SIZE);
    // Throws catchable exception
    // Detailed diagnostics
    // Works in release
}
```

---

## Migration Guide

### From assert()

Replace `assert()` calls based on their intent:

| Original | Intent | Replacement |
|----------|--------|-------------|
| `assert(ptr)` | Debug check | `enforce(ptr != nullptr, "...")` |
| `assert(ptr)` | Always check | `always_enforce_not_null(ptr, "...")` |
| `assert(x > 0)` | Debug check | `enforce(x > 0, "...")` |
| `assert(x > 0)` | Always check | `always_enforce_is_positive(x, "...")` |

### From Manual if-throw

```cpp
// Before
if (ptr == nullptr)
{
    throw std::invalid_argument("ptr is null");
}

// After
always_enforce_not_null(ptr, "ptr parameter");
```

```cpp
// Before
if (index >= container.size())
{
    throw std::out_of_range("index out of bounds");
}

// After
always_enforce_valid_index(index, container, "index");
```

---

## Best Practices

1. **Use `enforce()` for internal invariants** that should never fail if the code is correct.

2. **Use `always_enforce()` for public API preconditions** where callers might pass invalid arguments.

3. **Use `noexcept_enforce()` in destructors and noexcept functions** to avoid `std::terminate`.

4. **Use predicates for semantic clarity**:
   ```cpp
   // Clear intent
   always_enforce_is_positive(count, "count");
   
   // Less clear
   always_enforce(count > 0, "count must be positive");
   ```

5. **Include relevant values in error messages**:
   ```cpp
   always_enforce(index < size, "Index ", index, " >= size ", size);
   ```

6. **Set violation handler at program startup**:
   ```cpp
   int main()
   {
       fat_p::set_violation_handler(my_handler);
       // ...
   }
   ```

7. **Use contextual enforcement when noexcept behavior might vary**:
   ```cpp
   template <typename Func>
   void wrapper(Func&& f)
   {
       contextual_enforce(&wrapper<Func>, precondition(), "...");
       f();
   }
   ```

---

## Troubleshooting

### Compilation Errors

**Issue: "No matching function for call to enforce_1"**

```cpp
// Problem: Wrong number of predicate arguments
always_enforce_1(InRangePredicate, value, "Out of range");

// Solution: InRangePredicate takes 3 arguments
always_enforce_3(InRangePredicate, value, min, max, "Out of range");
```

**Issue: "static_assert failed: Predicate check requires numeric type"**

```cpp
// Problem: Using numeric predicate with non-numeric type
always_enforce_is_positive(ptr, "Must be positive");

// Solution: Use appropriate predicate
always_enforce_not_null(ptr, "Must not be null");
```

### Runtime Issues

**Issue: Exceptions thrown from noexcept functions**

```cpp
// Problem:
void my_func() noexcept
{
    always_enforce(condition, "msg");  // Throws! Calls std::terminate
}

// Solution: Use noexcept_enforce in noexcept functions
void my_func() noexcept
{
    noexcept_enforce(condition, "msg");  // Safe
}
```

**Issue: Checks not running in release build**

```cpp
// Problem: Defined NDEBUG, enforce() compiled out
enforce(critical_check(), "Must pass");  // Silent in release

// Solution: Use always_enforce for checks that must run
always_enforce(critical_check(), "Must pass");  // Always runs
```

**Issue: Custom handler not being called**

```cpp
// Problem: Handler set after violations already occurred
some_function_that_may_violate();
fat_p::set_violation_handler(my_handler);  // Too late!

// Solution: Set handler at program startup
int main()
{
    fat_p::set_violation_handler(my_handler);
    // ... rest of program
}
```

---

## Summary

### Key Features

- **Policy-based error handling**: throw, warn, abort, log, or return Expected
- **Rich predicates**: 25+ type-safe, reusable condition checks
- **Detailed diagnostics**: file, line, expression, and custom messages
- **Zero overhead option**: debug-only macros compile to nothing
- **Contextual awareness**: automatic adaptation to noexcept functions
- **Thread-safe**: all operations safe for concurrent use
- **Header-only**: no linking required, C++17 compatible

### Quick Reference

| Need | Macro |
|------|-------|
| Debug-only check | `enforce(cond, msg...)` |
| Always check | `always_enforce(cond, msg...)` |
| Warning only | `enforce_warn(cond, msg...)` |
| No throw | `noexcept_enforce(cond, msg...)` |
| Abort | `abort_enforce(cond, msg...)` |
| Return Expected | `enforce_expected(cond, msg...)` |
| Null check | `always_enforce_not_null(ptr, msg...)` |
| Range check | `always_enforce_in_range(min, max, val, msg...)` |
| Index check | `always_enforce_valid_index(idx, container, msg...)` |
| Contextual | `contextual_enforce(func_ptr, cond, msg...)` |

### Quick Start

```cpp
#include "enforce.h"

void process(int* data, size_t size)
{
    // Public API preconditions (always checked)
    always_enforce_not_null(data, "data parameter");
    always_enforce_is_positive(size, "size parameter");
    always_enforce(size <= MAX_SIZE, "size ", size, " exceeds max");
    
    // Internal invariants (debug only)
    enforce(is_aligned(data), "data not aligned");
    
    // Implementation...
}
```

### Exception Hierarchy

```mermaid
classDiagram
    class std_exception["std::exception"]
    class std_logic_error["std::logic_error"]
    class std_runtime_error["std::runtime_error"]
    class std_domain_error["std::domain_error"]
    class std_out_of_range["std::out_of_range"]
    class std_bad_alloc["std::bad_alloc"]
    
    class LogicContractError["fat_p::LogicContractError<br/>enforce, always_enforce"]
    class RuntimeContractError["fat_p::RuntimeContractError<br/>runtime checks"]
    class DomainContractError["fat_p::DomainContractError<br/>math domain errors"]
    class OutOfRangeContractError["fat_p::OutOfRangeContractError<br/>index/bounds errors"]
    class AllocContractError["fat_p::AllocContractError<br/>allocation failures"]
    
    std_exception <|-- std_logic_error
    std_exception <|-- std_runtime_error
    std_exception <|-- std_domain_error
    std_exception <|-- std_out_of_range
    std_exception <|-- std_bad_alloc
    
    std_logic_error <|-- LogicContractError
    std_runtime_error <|-- RuntimeContractError
    std_domain_error <|-- DomainContractError
    std_out_of_range <|-- OutOfRangeContractError
    std_bad_alloc <|-- AllocContractError
```

### Related Components

| Component | Purpose |
|-----------|---------|
| `ContractException.h` | Exception class hierarchy |
| `Expected.h` | Used by enforce_expected macros |
| `Stringify.h` | Type-to-string conversion for messages |
| `ConcurrencyPolicies.h` | Thread-safe violation handlers |

---

**Library:** fat_p C++ Utilities  
**Standard:** C++17  
**Type:** Header-only
