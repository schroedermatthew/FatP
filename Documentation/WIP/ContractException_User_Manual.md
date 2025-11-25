# ContractException User Manual

**Version:** 1.0  
**Library:** fat_p C++ Utilities  
**Standard:** C++17  
**Type:** Header-only

---

## Table of Contents

1. [Overview](#overview)
2. [Exception Hierarchy](#exception-hierarchy)
3. [Pre-defined Types](#pre-defined-types)
4. [Creating Custom Exceptions](#creating-custom-exceptions)
5. [Usage with Enforce](#usage-with-enforce)
6. [Catching Exceptions](#catching-exceptions)
7. [Best Practices](#best-practices)

---

## Overview

ContractException provides a hierarchy of exception types for Design-by-Contract (DbC) violations. These exceptions carry rich diagnostic information including the violation category and detailed messages.

### Include

```cpp
#include "ContractException.h"
using namespace fat_p;
```

### Key Features

- **Polymorphic base class**: `ContractViolationBase`
- **Category identification**: `category()` method
- **Standard library integration**: Inherits from `std::exception` subtypes
- **Pre-defined aliases**: Logic, Runtime, Allocation, Range, Domain errors

---

## Exception Hierarchy

```
std::exception
├── std::logic_error
│   └── ContractViolation<std::logic_error>  → LogicContractError
│
├── std::runtime_error
│   └── ContractViolation<std::runtime_error>  → RuntimeContractError
│
├── std::bad_alloc
│   └── ContractViolation<std::bad_alloc>  → AllocContractError
│
├── std::out_of_range
│   └── ContractViolation<std::out_of_range>  → OutOfRangeContractError
│
└── std::domain_error
    └── ContractViolation<std::domain_error>  → DomainContractError
```

### ContractViolationBase

Abstract base for all contract violations.

```cpp
class ContractViolationBase {
public:
    virtual ~ContractViolationBase() = default;
    
    // Get violation category (e.g., "Precondition", "Invariant")
    virtual std::string_view category() const noexcept = 0;
    
    // Get detailed message
    virtual std::string_view message() const noexcept = 0;
};
```

### ContractViolation<Base>

Template that creates contract exceptions inheriting from any `std::exception` subtype.

```cpp
template <typename BaseException>
class ContractViolation : public BaseException, 
                          public ContractViolationBase {
public:
    explicit ContractViolation(const std::string& msg, 
                               std::string_view cat = "Contract");
    
    // From std::exception
    const char* what() const noexcept override;
    
    // From ContractViolationBase
    std::string_view category() const noexcept override;
    std::string_view message() const noexcept override;
};
```

---

## Pre-defined Types

### LogicContractError

For precondition and invariant violations (programmer errors).

```cpp
// Throw
throw LogicContractError("Precondition failed: x must be positive");

// Catch
try {
    validate(input);
} catch (const LogicContractError& e) {
    std::cerr << "Logic error: " << e.what() << "\n";
}
```

**Use for**: Null pointers, invalid arguments, violated invariants.

### RuntimeContractError

For runtime failures that aren't programmer errors.

```cpp
// Throw
throw RuntimeContractError("Failed to connect to server");

// Catch
try {
    connect();
} catch (const RuntimeContractError& e) {
    log_error(e.what());
    retry();
}
```

**Use for**: I/O failures, resource unavailable, external service errors.

### AllocContractError

For allocation failures.

```cpp
// Throw
throw AllocContractError("Failed to allocate 1GB buffer");

// Catch
try {
    allocate_large_buffer();
} catch (const AllocContractError& e) {
    use_smaller_buffer();
}
```

**Use for**: Memory allocation failures, resource pool exhaustion.

### OutOfRangeContractError

For index and bounds violations.

```cpp
// Throw
throw OutOfRangeContractError("Index 10 out of range [0, 5)");

// Catch
try {
    access(index);
} catch (const OutOfRangeContractError& e) {
    std::cerr << "Bounds error: " << e.what() << "\n";
}
```

**Use for**: Array bounds, container access, range validation.

### DomainContractError

For mathematical domain errors.

```cpp
// Throw
throw DomainContractError("Cannot take sqrt of negative number");

// Catch
try {
    compute(value);
} catch (const DomainContractError& e) {
    return NAN;
}
```

**Use for**: Division by zero, sqrt of negative, log of non-positive.

---

## Creating Custom Exceptions

### Custom Category

```cpp
class PreconditionViolation : public LogicContractError {
public:
    explicit PreconditionViolation(const std::string& msg)
        : LogicContractError(msg) {}
    
    std::string_view category() const noexcept override {
        return "Precondition";
    }
};

class InvariantViolation : public LogicContractError {
public:
    explicit InvariantViolation(const std::string& msg)
        : LogicContractError(msg) {}
    
    std::string_view category() const noexcept override {
        return "Invariant";
    }
};

class PostconditionViolation : public LogicContractError {
public:
    explicit PostconditionViolation(const std::string& msg)
        : LogicContractError(msg) {}
    
    std::string_view category() const noexcept override {
        return "Postcondition";
    }
};
```

### Custom Base Exception

```cpp
// Create contract violation based on std::system_error
using SystemContractError = ContractViolation<std::system_error>;

// Create contract violation based on custom exception
class MyAppException : public std::exception { /* ... */ };
using AppContractError = ContractViolation<MyAppException>;
```

---

## Usage with Enforce

The enforce system uses these exceptions automatically:

```cpp
#include "enforce.h"

void process(int* data, size_t size) {
    // Throws LogicContractError if condition fails
    always_enforce(data != nullptr, "data must not be null");
    always_enforce(size > 0, "size must be positive");
    
    // ...
}
```

### Exception Selection

| Enforce Macro | Default Exception |
|---------------|-------------------|
| `enforce()` | `LogicContractError` |
| `always_enforce()` | `LogicContractError` |
| `abort_enforce()` | (calls `std::abort()`) |
| `enforce_warn()` | (no exception) |

---

## Catching Exceptions

### Catch Specific Type

```cpp
try {
    validate_and_process(input);
} catch (const OutOfRangeContractError& e) {
    // Handle bounds error specifically
    std::cerr << "Index error: " << e.what() << "\n";
} catch (const LogicContractError& e) {
    // Handle other logic errors
    std::cerr << "Logic error: " << e.what() << "\n";
}
```

### Catch by Category

```cpp
try {
    process();
} catch (const ContractViolationBase& e) {
    // Catch any contract violation
    std::cerr << "[" << e.category() << "] " << e.message() << "\n";
}
```

### Catch by Standard Type

```cpp
try {
    compute();
} catch (const std::logic_error& e) {
    // Catches LogicContractError (and other logic_errors)
    std::cerr << "Logic error: " << e.what() << "\n";
} catch (const std::runtime_error& e) {
    // Catches RuntimeContractError
    std::cerr << "Runtime error: " << e.what() << "\n";
}
```

### Recommended Pattern

```cpp
try {
    risky_operation();
} 
// Specific contract violations first
catch (const OutOfRangeContractError& e) {
    handle_bounds_error(e);
}
catch (const DomainContractError& e) {
    handle_math_error(e);
}
// Then general contract violations
catch (const ContractViolationBase& e) {
    log_contract_violation(e.category(), e.message());
    throw;  // Re-throw after logging
}
// Finally standard exceptions
catch (const std::exception& e) {
    handle_unexpected(e);
}
```

---

## Best Practices

### Do

```cpp
// ✅ Use appropriate exception type
throw OutOfRangeContractError("Index out of bounds");  // For bounds
throw DomainContractError("Division by zero");          // For math
throw LogicContractError("Null pointer");               // For preconditions

// ✅ Include context in messages
throw LogicContractError(
    "Precondition violated: buffer size " + std::to_string(size) + 
    " must be >= " + std::to_string(required)
);

// ✅ Catch by reference
catch (const LogicContractError& e) { /* ... */ }

// ✅ Use category() for logging
catch (const ContractViolationBase& e) {
    log("[", e.category(), "] ", e.message());
}
```

### Don't

```cpp
// ❌ Don't catch by value (slicing)
catch (LogicContractError e) { /* ... */ }  // Bad!

// ❌ Don't use generic messages
throw LogicContractError("error");  // Unhelpful!

// ❌ Don't catch and ignore
catch (const ContractViolationBase&) { }  // Silent failure!

// ❌ Don't use wrong exception type
throw RuntimeContractError("null pointer");  // Should be LogicContractError
```

---

## Integration Example

```cpp
#include "ContractException.h"
#include "enforce.h"
#include "Expected.h"

class DataProcessor {
public:
    Expected<Result, std::string> process(const Data& data) {
        // Preconditions (throw on violation)
        always_enforce(!data.empty(), "Data must not be empty");
        
        try {
            // May throw DomainContractError
            auto normalized = normalize(data);
            
            // May throw OutOfRangeContractError  
            auto result = compute(normalized);
            
            return result;
        }
        catch (const DomainContractError& e) {
            return make_unexpected("Math error: " + std::string(e.message()));
        }
        catch (const OutOfRangeContractError& e) {
            return make_unexpected("Bounds error: " + std::string(e.message()));
        }
    }
    
private:
    Data normalize(const Data& data) {
        for (auto& val : data) {
            if (val == 0) {
                throw DomainContractError("Cannot normalize: zero value found");
            }
        }
        // ...
    }
    
    Result compute(const Data& data) {
        // ...
    }
};
```

---

## Related Components

- **enforce.h**: Macros that throw these exceptions
- **Expected.h**: Alternative to exceptions for error handling
- **Stringify.h**: For building error messages

---

**Document Version:** 1.0  
**Last Updated:** November 2025
