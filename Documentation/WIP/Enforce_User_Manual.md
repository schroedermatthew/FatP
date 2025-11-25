# Enforce System User Manual

**Version:** 2.0  
**Library:** fat_p C++ Utilities  
**Standard:** C++17  
**Type:** Header-only

---

## Table of Contents

1. [Overview](#overview)
2. [Quick Start](#quick-start)
3. [Core Macros](#core-macros)
4. [Policies](#policies)
5. [Predicates](#predicates)
6. [Error Messages](#error-messages)
7. [Expected Integration](#expected-integration)
8. [Contextual Enforcement](#contextual-enforcement)
9. [Custom Policies](#custom-policies)
10. [Best Practices](#best-practices)
11. [Performance](#performance)

---

## Overview

The Enforce System provides Design-by-Contract (DbC) enforcement with:
- **Policy-based error handling**: throw, warn, abort, log, or return Expected
- **Rich predicates**: type-safe condition checking
- **Detailed diagnostics**: file, line, expression, and custom messages
- **Zero overhead in release**: debug-only macros compile to nothing

### Files

| Header | Purpose |
|--------|---------|
| `enforce.h` | Main macros |
| `enforce_predicates.h` | Predicate policies |
| `enforce_raisers.h` | Error handling policies |
| `enforce_enforcers.h` | RAII enforcer class |
| `enforce_contextual.h` | Scoped enforcement |
| `ContractException.h` | Exception types |

### Include

```cpp
#include "enforce.h"

// For contextual enforcement
#include "enforce_contextual.h"
```

---

## Quick Start

### Basic Enforcement

```cpp
#include "enforce.h"
using namespace fat_p;

void process(int* ptr, int count) {
    // Debug-only checks (compiled out in release)
    enforce(ptr != nullptr, "Pointer must not be null");
    enforce(count > 0, "Count must be positive: ", count);
    
    // Always-active checks
    always_enforce(count <= MAX_COUNT, "Count exceeds maximum");
    
    // ... implementation
}
```

### With Predicates

```cpp
void send_data(const std::vector<int>& data, int* dest) {
    always_enforce_1(NotNullPredicate, dest, "Destination is null");
    always_enforce_1(NotEmptyPredicate, data, "Data is empty");
    
    // Process...
}
```

### Expected-Based (No Exceptions)

```cpp
Expected<void, std::string> validate(int value) {
    auto result = enforce_expected(value > 0, "Value must be positive");
    if (!result) return result;
    
    result = enforce_expected(value < 100, "Value must be < 100");
    if (!result) return result;
    
    return {};  // Success
}
```

---

## Core Macros

### Condition Macros

| Macro | Active In | On Failure |
|-------|-----------|------------|
| `enforce(cond, ...)` | Debug only | Throws `LogicContractError` |
| `always_enforce(cond, ...)` | Always | Throws `LogicContractError` |
| `enforce_warn(cond, ...)` | Always | Logs to stderr |
| `noexcept_enforce(cond, ...)` | Always | Logs, no throw |
| `abort_enforce(cond, ...)` | Always | Calls `std::abort()` |

### Expected Macros

| Macro | Returns |
|-------|---------|
| `enforce_expected(cond, ...)` | `Expected<void, std::string>` |
| `always_enforce_expected(cond, ...)` | `Expected<void, std::string>` |

### Predicate Macros

```cpp
// Single argument predicates
always_enforce_1(PredicateType, target, message...)
enforce_1(PredicateType, target, message...)

// Two argument predicates
always_enforce_2(PredicateType, arg1, arg2, message...)

// Three argument predicates
always_enforce_3(PredicateType, arg1, arg2, arg3, message...)
```

---

## Policies

### DebugOnlyPolicy (Default for `enforce`)

- Active only when `NDEBUG` is **not** defined
- Zero overhead in release builds
- Throws `LogicContractError` on failure

```cpp
enforce(ptr != nullptr);  // Only checked in debug
```

### AlwaysEnforcePolicy

- Active in both debug and release builds
- Throws `LogicContractError` on failure

```cpp
always_enforce(user_input.size() < MAX_SIZE);
```

### WarningPolicy

- Logs to stderr but continues execution
- Never throws

```cpp
enforce_warn(cache_hit, "Cache miss for key: ", key);
```

### NoThrowPolicy

- Logs error but never throws
- Useful in destructors and noexcept functions

```cpp
~Resource() {
    noexcept_enforce(handle_ != INVALID, "Invalid handle in destructor");
    release(handle_);
}
```

### AbortPolicy

- Calls `std::abort()` immediately
- For unrecoverable errors

```cpp
abort_enforce(googole_connection, "Lost connection to critical service");
```

---

## Predicates

### Core Predicates

| Predicate | Checks |
|-----------|--------|
| `BooleanPredicate` | Condition is true |
| `NotNullPredicate` | Pointer is not null |
| `NotEmptyPredicate` | Container is not empty |
| `IsPositivePredicate` | Value > 0 |
| `IsNonNegativePredicate` | Value >= 0 |
| `IsIntegralPredicate` | Type is integral |

### Container Predicates

| Predicate | Checks |
|-----------|--------|
| `ContainerIsUniquePredicate` | No duplicate elements |
| `ContainerIsSortedPredicate` | Elements are sorted |
| `ContainerHasElementPredicate` | Contains specific element |

### Range Predicates

| Predicate | Checks |
|-----------|--------|
| `InRangePredicate` | Value in [min, max] |
| `InExclusiveRangePredicate` | Value in (min, max) |
| `ValidIndexPredicate` | Index < container.size() |

### Floating-Point Predicates

| Predicate | Checks |
|-----------|--------|
| `IsFinitePredicate` | Not NaN or Inf |
| `IsNormalPredicate` | Not denormalized |
| `ApproxEqualPredicate` | Within epsilon |

### Usage Examples

```cpp
// Null check
always_enforce_1(NotNullPredicate, ptr, "Null pointer");

// Range check
always_enforce_3(InRangePredicate, value, 0, 100, "Value out of range");

// Valid index
always_enforce_2(ValidIndexPredicate, idx, vec, "Index out of bounds");

// Sorted container
always_enforce_1(ContainerIsSortedPredicate, data, "Data must be sorted");

// Floating-point sanity
always_enforce_1(IsFinitePredicate, result, "Result is NaN or Inf");
```

---

## Error Messages

### Message Formatting

Messages support variadic arguments with automatic stringification:

```cpp
enforce(x > 0, "Invalid x: ", x, " (expected positive)");
// Output: Invalid x: -5 (expected positive)

enforce(ptr != nullptr, 
    "Null pointer at index ", idx, " in array of size ", size);
```

### Full Error Format

```
Contract Violation:
    Condition: x > 0
    Locus: file.cpp:42
    Message: Invalid x: -5 (expected positive)
```

### Custom Stringification

Types are converted using `Stringify.h`:

```cpp
struct Point { int x, y; };

// Provide operator<< for custom types
std::ostream& operator<<(std::ostream& os, const Point& p) {
    return os << "(" << p.x << ", " << p.y << ")";
}

Point p{10, 20};
enforce(p.x >= 0, "Invalid point: ", p);
// Message: Invalid point: (10, 20)
```

---

## Expected Integration

### Basic Pattern

```cpp
Expected<int, std::string> parse_positive(const std::string& s) {
    auto result = enforce_expected(!s.empty(), "Empty input");
    if (!result) return make_unexpected(result.error());
    
    int value;
    try {
        value = std::stoi(s);
    } catch (...) {
        return make_unexpected("Invalid integer: " + s);
    }
    
    result = enforce_expected(value > 0, "Not positive: ", value);
    if (!result) return make_unexpected(result.error());
    
    return value;
}
```

### Chained Validation

```cpp
Expected<Config, std::string> load_config(const std::string& path) {
    return enforce_expected(!path.empty(), "Empty path")
        .and_then([&]() { return read_file(path); })
        .and_then([](const std::string& content) { return parse_json(content); })
        .and_then([](const Json& json) { return validate_config(json); });
}
```

---

## Contextual Enforcement

The `enforce_contextual.h` header provides scoped enforcement with context:

```cpp
#include "enforce_contextual.h"

void process_request(const Request& req) {
    ENFORCE_CONTEXT("Processing request", req.id);
    
    // All enforce calls within this scope include context
    enforce(req.valid(), "Invalid request");
    // Error includes: "Context: Processing request [id=123]"
    
    {
        ENFORCE_CONTEXT("Validating payload");
        enforce(req.payload.size() < MAX_SIZE);
        // Error includes both contexts
    }
}
```

### Context Stack

```cpp
ENFORCE_CONTEXT("Outer", value1);
{
    ENFORCE_CONTEXT("Inner", value2);
    enforce(condition);
    // Error shows: "Context: Outer [value1] > Inner [value2]"
}
```

---

## Custom Policies

### Custom Predicate

```cpp
struct EvenNumberPredicate {
    template <typename T>
    static constexpr bool check(T value) noexcept {
        return value % 2 == 0;
    }
};

// Usage
always_enforce_1(EvenNumberPredicate, count, "Count must be even");
```

### Custom Raiser

```cpp
struct LogAndContinueRaiser {
    static void fail(const std::string& message) {
        my_logger::error(message);
        // Don't throw - just log and continue
    }
};

// Register with the system (advanced usage)
template <>
struct RaiserSelector<MyCustomPolicy> {
    using type = LogAndContinueRaiser;
};
```

### Custom Violation Handler

```cpp
// Global handler for all violations
fat_p::set_violation_handler([](const std::string& msg) {
    send_to_monitoring(msg);
    log_to_file(msg);
    // Default behavior follows
});
```

---

## Best Practices

### Do

```cpp
// ✅ Use enforce for internal invariants
enforce(internal_state_valid());

// ✅ Use always_enforce for public API preconditions
void public_api(int* data, size_t size) {
    always_enforce(data != nullptr, "data must not be null");
    always_enforce(size > 0, "size must be positive");
}

// ✅ Use descriptive messages
enforce(idx < vec.size(), 
    "Index ", idx, " out of bounds for vector of size ", vec.size());

// ✅ Use predicates for type-safe checks
always_enforce_1(NotNullPredicate, ptr);
always_enforce_2(ValidIndexPredicate, idx, container);

// ✅ Use enforce_expected in noexcept contexts
Expected<void, std::string> init() noexcept {
    return enforce_expected(setup_complete, "Setup failed");
}
```

### Don't

```cpp
// ❌ Don't use enforce for expected failures
enforce(file.exists());  // Use proper error handling

// ❌ Don't put side effects in conditions
enforce(init_system());  // Side effect removed in release!

// ❌ Don't use enforce for user input validation
enforce(user_input.valid());  // Should handle gracefully

// ❌ Don't leave messages empty
enforce(ptr != nullptr);  // Add context!
```

---

## Performance

### Debug vs Release

| Macro | Debug Cost | Release Cost |
|-------|------------|--------------|
| `enforce` | ~5-20ns | **0** (compiled out) |
| `always_enforce` | ~5-20ns | ~5-20ns |
| `enforce_warn` | ~100ns (I/O) | ~100ns |
| `abort_enforce` | ~5ns | ~5ns |

### Optimization Tips

```cpp
// Expensive check - wrap in debug-only
enforce([&] {
    return expensive_validation(data);
}(), "Validation failed");

// In release, the lambda is never called
```

### String Allocation

Messages are only constructed on failure:

```cpp
// Good: no allocation if condition passes
enforce(x > 0, "Value ", x, " is not positive");

// The stringification only happens when x <= 0
```

---

## Exception Types

| Exception | Used For |
|-----------|----------|
| `LogicContractError` | Precondition/invariant violations |
| `RuntimeContractError` | Runtime failures |
| `AllocContractError` | Allocation failures |
| `OutOfRangeContractError` | Index/bounds violations |
| `DomainContractError` | Mathematical domain errors |

### Catching

```cpp
try {
    process();
} catch (const ContractViolationBase& e) {
    // Catch any contract violation
    std::cerr << "Contract: " << e.category() << ": " << e.message();
} catch (const LogicContractError& e) {
    // Catch specific type
} catch (const std::logic_error& e) {
    // LogicContractError also caught here (inheritance)
}
```

---

## Related Components

- **ContractException.h**: Exception class hierarchy
- **Expected.h**: Used by `enforce_expected` macros
- **Stringify.h**: Type-to-string conversion for messages
- **ConcurrencyPolicies.h**: Thread-safe violation handlers

---

**Document Version:** 1.0  
**Last Updated:** November 2025
