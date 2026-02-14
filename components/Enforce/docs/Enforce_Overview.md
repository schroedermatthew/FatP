# Enforce: A Fat-P Library Showcase

## Executive Summary

Enforce is a **policy-based contract enforcement system** that separates WHAT you check and HOW you raise errors through orthogonal compile-time composition. Unlike `assert()` (disabled in release, no customization) or manual `if/throw` (scattered, inconsistent), Enforce composes **predicates** and **raisers** into zero-overhead contract checks. The `if constexpr` dispatch eliminates dead code paths, generating assembly identical to hand-written checks while providing architectural flexibility for debug/release/test configurations.

---

## The Problem Domain

### What Goes Wrong Without It

```cpp
// The assert trap: disabled in release builds
void process(int* ptr, size_t size) {
    assert(ptr != nullptr);  // Compiled out with -DNDEBUG!
    assert(size > 0);        // Silent corruption in production
    // ...
}

// The scattered throw pattern: inconsistent, verbose
void validate(const Config& cfg) {
    if (cfg.threads < 1) {
        throw std::invalid_argument("threads must be >= 1");
    }
    if (cfg.timeout <= 0) {
        throw std::invalid_argument("timeout must be > 0");
    }
    // Repeat for every function, every parameter
    // Different message formats, different exception types
}

// The logging-vs-throwing dilemma
void critical_path(Data& data) {
    if (!data.valid()) {
        // Do I log? Throw? Abort? Return error?
        // Different answer in debug vs. production vs. testing
    }
}
```

| Issue | HPC Impact |
|-------|------------|
| `assert()` disabled in release | Contract violations go undetected in production |
| Scattered `if/throw` | Inconsistent error handling across codebase |
| No policy flexibility | Can't switch between throw/log/abort per context |
| Verbose checks | Obscures business logic with validation boilerplate |

### The Standard's Limitation

C++20 introduced contracts (`[[expects:]]`, `[[ensures:]]`) but they were removed before standardization. C++23 has no contract facility. The only standard option is `assert()`, which:
- Is disabled by `NDEBUG`
- Calls `abort()` (no stack unwinding, no cleanup)
- Cannot be customized per-context
- Has no predicate composition

---

## Architecture: Orthogonal Policy Composition

### The Mechanism: Two Axes of Customization

```cpp
// Axis 1: PREDICATES - What to check
template<typename T>
struct not_null {
    static bool check(const T& ptr) { return ptr != nullptr; }
    static const char* message() { return "Pointer must not be null"; }
};

template<typename T>
struct positive {
    static bool check(const T& val) { return val > 0; }
    static const char* message() { return "Value must be positive"; }
};

// Axis 2: RAISERS - How to report violations
struct throw_raiser {
    [[noreturn]] static void raise(const char* msg, const char* file, int line) {
        throw contract_violation(msg, file, line);
    }
};

struct abort_raiser {
    [[noreturn]] static void raise(const char* msg, const char* file, int line) {
        std::cerr << file << ":" << line << ": " << msg << "\n";
        std::abort();
    }
};

struct log_raiser {
    static void raise(const char* msg, const char* file, int line) {
        log_error(file, line, msg);
        // Continues execution
    }
};
```

### The Core Function

```cpp
template<typename Predicate, typename Raiser, typename... Args>
constexpr void enforce(Args&&... args) {
    if constexpr (Raiser::enabled) {
        if (!Predicate::check(std::forward<Args>(args)...)) {
            Raiser::raise(Predicate::message(), __FILE__, __LINE__);
        }
    }
    // When disabled: entire function body is empty
}
```

**Generated code when disabled:** The `if constexpr (false)` branch is eliminated entirely. Zero instructions generated.

**Generated code when enabled:**
```asm
; enforce<not_null, throw_raiser>(ptr)
test    rdi, rdi          ; Check ptr != nullptr
jz      .throw_violation  ; Jump if null
ret                       ; Continue if valid
```

---

## Feature Inventory

### 1. Predefined Predicates

```cpp
// Null checks
enforce<not_null>(ptr);                    // ptr != nullptr
enforce<not_null>(span);                   // !span.empty()

// Numeric checks
enforce<positive>(count);                  // count > 0
enforce<non_negative>(index);              // index >= 0
enforce<in_range<0, 100>>(percent);        // 0 <= percent <= 100

// Container checks
enforce<not_empty>(container);             // !container.empty()
enforce<size_at_least<5>>(container);      // container.size() >= 5

// Custom predicates
enforce<my_predicate>(value);              // my_predicate::check(value)
```

### 2. Raiser Options

```cpp
// FATP_ALWAYS_ENFORCE: Always enabled, throws
FATP_ALWAYS_ENFORCE(condition, "Message");

// FATP_DEBUG_ENFORCE: Enabled in debug, disabled in release
FATP_DEBUG_ENFORCE(condition, "Debug-only check");

// Custom raiser
enforce<predicate, log_and_continue>(value);
enforce<predicate, throw_raiser>(value);
enforce<predicate, abort_raiser>(value);
```

### 3. Expression-Based Enforcement

```cpp
// enforce_that: Captures expression text for error message
enforce_that(x > 0);          // "Violation: x > 0"
enforce_that(ptr != nullptr); // "Violation: ptr != nullptr"

// With custom message
enforce_that(x > 0, "x must be positive for this algorithm");
```

### 4. Multi-Condition Enforcement

```cpp
// enforce_all: All conditions must pass
enforce_all<not_null, not_empty>(ptr, container);

// enforce_any: At least one condition must pass
enforce_any<has_value, has_default>(optional, default_value);
```

---

## Why Not Alternatives?

| If You Need... | Why Not assert() | Why Not if/throw | Why Not Boost.Assert | Fat-P Advantage |
|----------------|------------------|------------------|---------------------|-----------------|
| Release builds | âŒ Disabled by NDEBUG | âœ… Works | âœ… Works | âœ… FATP_ALWAYS_ENFORCE |
| Policy selection | âŒ Fixed abort | âŒ Fixed throw | Limited | âœ… Raiser + Context |
| Predicate composition | âŒ Manual | âŒ Manual | âŒ Manual | âœ… Composable |
| Zero-overhead disable | âœ… Compiled out | âŒ Always present | âœ… Compiled out | âœ… if constexpr |
| Expression capture | âŒ No | âŒ Manual | Partial | âœ… enforce_that |

**The Sweet Spot:** Enforce is the only option combining predicate composition, raiser customization, and zero-overhead disabling.

---

## The "Forever Stuck" Reality

**Standard Reality:** C++20 contracts were removed. C++23/26 may revive them, but:
- Design is contentious (build modes, continuation semantics)
- Even when standardized, may not offer policy customization
- No composition of predicates planned

Enforce provides contracts **today** with architectural flexibility the standard will never match because different codebases need different violation responses (throw in libraries, abort in kernels, log in production).

---

## Performance Characteristics

| Scenario | Cost | Notes |
|----------|------|-------|
| Check enabled, passes | ~0.5-2 ns | Branch prediction usually correct |
| Check enabled, fails | Raiser cost | Exception: ~1000+ ns; Abort: ~100 ns |
| Check disabled | 0 ns | Entire check compiled out |

### Code Generation Comparison

```cpp
// assert (NDEBUG undefined)
assert(ptr != nullptr);
// â†’ if (!ptr) { __assert_fail(...); }

// Enforce (always enabled)
FATP_ALWAYS_ENFORCE(ptr != nullptr, "...");
// â†’ if (!ptr) { throw contract_violation(...); }

// Enforce (disabled)
FATP_DEBUG_ENFORCE<my_policy>(ptr != nullptr, "...");
// â†’ (nothing - entire check eliminated)
```

### Where Fat-P Wins
- Production systems needing runtime contract checks
- Libraries with configurable error handling
- Debug/release builds needing different violation behavior

### Where Fat-P Loses (Honesty Builds Trust)
- Simple debugging â†’ `assert()` is more familiar
- No customization needed â†’ `if/throw` is simpler
- Contract expressions only â†’ wait for standard contracts

---

## Integration Points

```
enforce.h
    â†“ components
enforce_predicates.h     (not_null, positive, in_range, ...)
enforce_raisers.h        (throw_raiser, abort_raiser, log_raiser)
    â†“ used by
SmallVector.h            (bounds checking)
CheckedArithmetic.h      (ThrowOnErrorPolicy)
Expected.h               (bad_expected_access)
Every fat_p component    (input validation)
```

---

## Final Assessment

Enforce delivers on the fat_p promise through three pillars:

### 1. Permanence
C++20 contracts were removed. Future standards may add them but won't offer policy-based customization. Enforce provides contracts **now** with flexibility the standard cannot match.

### 2. Specialization
Three orthogonal axes (predicate, raiser, context) compose into exactly the behavior you need. Debug builds throw; production aborts; tests log. Same predicate, different responses.

### 3. Control
You choose what to check (predicate), how to report (raiser), and where policies apply (context). `if constexpr` ensures zero overhead when disabled. The policy IS the contract enforcement.

**Architectural Verdict:** Enforce transforms contract checking from **scattered if/throw** or **disabled assert()** to **composable, policy-based, zero-overhead** enforcement. It's Design by Contract for C++, done right.

---

*enforce.h â€” Fat-P Library*
