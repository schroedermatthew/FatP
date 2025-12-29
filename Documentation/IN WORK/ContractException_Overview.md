# ContractException: A Fat-P Library Showcase

## Executive Summary

ContractException provides **polymorphic contract violation exceptions** that inherit from appropriate standard exception bases (`std::logic_error`, `std::runtime_error`, `std::bad_alloc`) while sharing a common `ContractViolationBase` interface. Unlike single-base exception hierarchies (catch-all or miss-specific), ContractException enables **dual-hierarchy catching**: catch all contract violations uniformly via `ContractViolationBase`, OR catch specific standard categories via `std::logic_error` etc. This diamond-free design separates classification from categorization.

---

## The Problem Domain

### What Goes Wrong Without It

```cpp
// The single-hierarchy trap
class ContractError : public std::runtime_error { /*...*/ };
class PreconditionError : public ContractError { /*...*/ };
class AllocationError : public ContractError { /*...*/ };

// Problem: AllocationError doesn't inherit from std::bad_alloc
// Generic code expecting std::bad_alloc won't catch it!
try {
    allocate_with_contract();
} catch (const std::bad_alloc&) {
    // Never catches AllocationError!
}

// The diamond inheritance trap
class BadContractError : public std::bad_alloc, public ContractError {
    // Diamond: both inherit from std::exception
    // Multiple what() implementations, ambiguous catch
};
```

| Issue | HPC Impact |
|-------|------------|
| Single hierarchy | Can't catch both contract violations AND standard categories |
| Diamond inheritance | Ambiguous `what()`, slicing risks |
| No unified interface | Can't log all contract violations uniformly |
| Lost category info | Can't distinguish precondition vs. allocation failure |

### The Standard's Limitation

The C++ standard exception hierarchy is fixed:
- `std::logic_error` for programmer errors (preconditions)
- `std::runtime_error` for external failures
- `std::bad_alloc` for allocation failures

You can inherit from ONE of these. If you want contract violations that are ALSO proper standard exceptions, you need a custom design.

---

## Architecture: Dual-Hierarchy via Mixin

### The Mechanism: Separate Base + Standard Inheritance

```cpp
// Common interface for ALL contract violations
class ContractViolationBase {
public:
    virtual ~ContractViolationBase() = default;
    virtual const char* category() const noexcept = 0;
    virtual const char* message() const noexcept = 0;
};

// Logic errors (precondition/postcondition violations)
class LogicContractError 
    : public std::logic_error,          // IS-A logic_error
      public ContractViolationBase      // IS-A contract violation
{
public:
    explicit LogicContractError(const std::string& msg)
        : std::logic_error("Contract Violation: " + msg) {}
    
    const char* category() const noexcept override { return "Logic"; }
    const char* message() const noexcept override { return what(); }
};

// Allocation errors
class AllocContractError
    : public std::bad_alloc,            // IS-A bad_alloc
      public ContractViolationBase      // IS-A contract violation
{
    std::string message_;
public:
    explicit AllocContractError(const std::string& msg) : message_(msg) {}
    const char* what() const noexcept override { return message_.c_str(); }
    const char* category() const noexcept override { return "Allocation"; }
    const char* message() const noexcept override { return what(); }
};
```

**Why no diamond:** `ContractViolationBase` does NOT inherit from `std::exception`. Each concrete class inherits from exactly one `std::exception` subclass plus the mixin interface.

### Catching Strategies

```cpp
// Strategy 1: Catch any contract violation
try {
    some_contract_enforced_code();
} catch (const ContractViolationBase& e) {
    log("Contract violation [", e.category(), "]: ", e.message());
}

// Strategy 2: Catch standard categories (generic code works)
try {
    allocate_checked();
} catch (const std::bad_alloc& e) {
    // Catches AllocContractError!
    handle_oom();
}

// Strategy 3: Catch specific contract types
try {
    validate_input();
} catch (const LogicContractError& e) {
    // Precondition failure
} catch (const RuntimeContractError& e) {
    // External failure (file not found, etc.)
}
```

---

## Feature Inventory

### 1. LogicContractError (Preconditions/Postconditions)

```cpp
void process(int* ptr, size_t size) {
    if (ptr == nullptr) {
        throw LogicContractError("ptr must not be null");
    }
    if (size == 0) {
        throw LogicContractError("size must be > 0");
    }
}

// Catchable as:
// - const LogicContractError&
// - const std::logic_error&
// - const std::exception&
// - const ContractViolationBase&
```

### 2. RuntimeContractError (External Failures)

```cpp
Expected<Config, Error> loadConfig(const std::string& path) {
    std::ifstream file(path);
    if (!file) {
        throw RuntimeContractError("File not found: " + path);
    }
}

// Catchable as:
// - const RuntimeContractError&
// - const std::runtime_error&
// - const std::exception&
// - const ContractViolationBase&
```

### 3. AllocContractError (Memory Allocation)

```cpp
void* allocate(size_t size) {
    void* ptr = std::malloc(size);
    if (!ptr) {
        throw AllocContractError("Failed to allocate " + std::to_string(size) + " bytes");
    }
    return ptr;
}

// Catchable as:
// - const AllocContractError&
// - const std::bad_alloc&
// - const std::exception&
// - const ContractViolationBase&
```

### 4. OutOfRangeContractError (Bounds Violations)

```cpp
template<typename T>
T& SmallVector<T>::at(size_t index) {
    if (index >= size_) {
        throw OutOfRangeContractError("Index " + std::to_string(index) + 
                                       " out of range [0, " + std::to_string(size_) + ")");
    }
    return data_[index];
}

// Catchable as:
// - const OutOfRangeContractError&
// - const std::out_of_range&
// - const std::logic_error&
// - const std::exception&
// - const ContractViolationBase&
```

### 5. Stream Output Support

```cpp
try {
    risky_operation();
} catch (const ContractViolationBase& e) {
    std::cerr << e;  // operator<< overloaded
    // Output: "[Logic] Contract Violation: ptr must not be null"
}
```

---

## Why Not Alternatives?

| If You Need... | Why Not std::logic_error | Why Not Custom Hierarchy | Why Not Boost.Exception | Fat-P Advantage |
|----------------|--------------------------|-------------------------|------------------------|-----------------|
| Unified contract catching | ❌ No common interface | ✅ Works | ✅ Works | ✅ Works |
| Standard category catching | ✅ Works | ❌ Lost | ❌ Lost | ✅ Dual hierarchy |
| bad_alloc compatibility | ❌ Different hierarchy | ❌ Needs diamond | ❌ Different design | ✅ Clean mixin |
| Zero dependencies | ✅ Standard | ✅ Custom | ❌ Requires Boost | ✅ Single header |
| Category metadata | ❌ No | Manual | ❌ Complex | ✅ Built-in |

**The Sweet Spot:** ContractException is the only option providing both unified contract catching AND standard exception category compatibility without diamond inheritance.

---

## The "Forever Stuck" Reality

**Standard Reality:** The C++ exception hierarchy is fixed and will not change:
- No common base for "contract violations"
- No mixin-style exception extensions
- Diamond inheritance remains problematic

ContractException provides a pattern for extending standard exceptions with custom interfaces—permanently useful regardless of future C++ standards.

---

## Performance Characteristics

| Scenario | Cost | Notes |
|----------|------|-------|
| No exception thrown | 0 ns | Zero overhead on happy path |
| Exception construction | ~50-200 ns | String allocation dominates |
| Throw + catch | ~1-5 μs | Stack unwinding dominates |
| `what()` call | ~1 ns | Virtual call + string return |
| `category()` call | ~1 ns | Virtual call + constant return |

### Memory Overhead

| Component | Size | Notes |
|-----------|------|-------|
| vtable pointer | 8 bytes | Standard |
| std::string message | ~32 bytes | SSO or heap |
| Total per instance | ~40-48 bytes | Typical |

### Where Fat-P Wins
- Unified logging of all contract violations
- Generic code expecting standard exceptions
- Libraries needing both contract and standard semantics

### Where Fat-P Loses (Honesty Builds Trust)
- Simple projects → plain `std::runtime_error` suffices
- No exception policy → use Enforce with non-throwing raisers
- Extremely tight memory → exception objects have overhead

---

## Integration Points

```
ContractException.h
    ↓ used by
enforce.h           (LogicRaiser, RuntimeRaiser throw these)
SmallVector.h       (OutOfRangeContractError for at())
CheckedArithmetic.h (LogicContractError for overflow)
Factory.h           (RuntimeContractError for creation failure)
```

---

## Final Assessment

ContractException delivers on the fat_p promise through three pillars:

### 1. Permanence
C++ exception hierarchy won't gain a "contract violation" concept. The dual-hierarchy mixin pattern provides unified catching permanently.

### 2. Specialization
Four exception types match standard categories (logic, runtime, allocation, bounds) while sharing contract semantics. Category metadata enables structured logging.

### 3. Control
Catch-site decides granularity: catch all violations, catch by standard category, or catch specific contract types. The dual hierarchy enables all three.

**Architectural Verdict:** ContractException transforms exception handling from **single-hierarchy limitations** to **dual-hierarchy flexibility**—catch contract violations uniformly OR by standard category, without diamond inheritance problems.

---

*ContractException.h (281 lines) — Fat-P Library*
