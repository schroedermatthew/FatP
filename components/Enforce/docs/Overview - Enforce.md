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
// Axis 1: PREDICATES - What to check (structs with a static check())
struct NotNullPredicate {
    template <typename Ptr>
    static constexpr bool check(const Ptr& ptr) noexcept { return ptr != nullptr; }
};

struct IsPositivePredicate {
    template <typename T>
    static constexpr bool check(T value) noexcept { return value > T{0}; }
};

// Axis 2: RAISERS - How to report violations (structs with a static fail())
struct LogicRaiser {           // throws LogicContractError
    [[noreturn]] static void fail(const std::string& message);
};

struct AbortRaiser {           // logs to stderr, then std::abort()
    [[noreturn]] static void fail(const std::string& message) noexcept;
};

struct WarningToCerrRaiser {   // logs to stderr, execution continues
    static void fail(const std::string& message) noexcept;
};
```

### The Core Function

The composition point is `enforce_policy_impl`, which selects a raiser from a policy and wraps the check result; the `FATP_*` macros stringify the condition and capture `std::source_location` at the call site:

```cpp
template <typename Policy>
[[nodiscard]] constexpr auto enforce_policy_impl(bool passed,
                                                 const char* expression_str,
                                                 std::source_location loc)
{
    using Raiser = typename RaiserSelector<Policy>::type;
    return MakeEnforcer<Raiser>(passed, expression_str, loc);
}
```

**Generated code when disabled:** In release builds (`NDEBUG`), `FATP_ENFORCE` expands to `((void)0)` — preprocessor elimination, zero instructions generated.

**Generated code when enabled:**
```asm
; FATP_ALWAYS_ENFORCE_NOT_NULL(ptr)
test    rdi, rdi          ; Check ptr != nullptr
jz      .throw_violation  ; Jump if null
ret                       ; Continue if valid
```

---

## Feature Inventory

### 1. Predefined Predicates (via the predicate macros)

Predicates are structs in `enforce_predicates.h` invoked through the `FATP_*_ENFORCE_1/2/3` macros (suffix = argument count) or through named convenience macros:

```cpp
// Null checks
FATP_ALWAYS_ENFORCE_NOT_NULL(ptr);                     // ptr != nullptr
FATP_ALWAYS_ENFORCE_1(NotNullPredicate, ptr);          // same, explicit form

// Numeric checks
FATP_ALWAYS_ENFORCE_IS_POSITIVE(count);                // count > 0
FATP_ALWAYS_ENFORCE_IS_NON_NEGATIVE(index);            // index >= 0
FATP_ALWAYS_ENFORCE_IN_RANGE(0, 100, percent);         // 0 <= percent <= 100

// Container checks
FATP_ALWAYS_ENFORCE_NOT_EMPTY(container);              // !container.empty()
FATP_ALWAYS_ENFORCE_HAS_SIZE(5, container);            // container.size() == 5

// Custom predicates: any struct with a static check()
FATP_ALWAYS_ENFORCE_1(MyPredicate, value);             // MyPredicate::check(value)
```

### 2. Raiser/Policy Options

```cpp
// FATP_ALWAYS_ENFORCE: Always enabled, throws LogicContractError
FATP_ALWAYS_ENFORCE(condition, "Message");

// FATP_ENFORCE: Enabled in debug, expands to nothing in release
FATP_ENFORCE(condition, "Debug-only check");

// Other responses
FATP_ENFORCE_WARN(condition, "...");      // WarningToCerrRaiser: log + continue
FATP_ABORT_ENFORCE(condition, "...");     // AbortRaiser: log + std::abort()
FATP_NOEXCEPT_ENFORCE(condition, "...");  // NoThrowRaiser: safe in noexcept code
```

### 3. Expression Capture

Every macro stringifies its condition (`#condition`) and captures `std::source_location`, so the diagnostic carries the expression text, file, line, and function automatically:

```cpp
FATP_ALWAYS_ENFORCE(x > 0);   // message includes "x > 0" + call site
FATP_ALWAYS_ENFORCE(x > 0, "x must be positive for this algorithm");
```

### 4. Container-Wide Enforcement

```cpp
// All elements must satisfy a predicate
FATP_ALWAYS_ENFORCE_ALL_SATISFY(is_valid, container);

// At least one element must satisfy a predicate
FATP_ALWAYS_ENFORCE_ANY_SATISFY(is_ready, container);
```

### 5. Expected Integration

```cpp
// Returns Expected<void, std::string> instead of throwing
auto result = FATP_ENFORCE_EXPECTED(size > 0, "size must be positive");
```

---

## Why Not Alternatives?

| If You Need... | Why Not assert() | Why Not if/throw | Why Not Boost.Assert | Fat-P Advantage |
|----------------|------------------|------------------|---------------------|-----------------|
| Release builds | âŒ Disabled by NDEBUG | âœ… Works | âœ… Works | âœ… FATP_ALWAYS_ENFORCE |
| Policy selection | âŒ Fixed abort | âŒ Fixed throw | Limited | âœ… Raiser + Context |
| Predicate composition | âŒ Manual | âŒ Manual | âŒ Manual | âœ… Composable |
| Zero-overhead disable | âœ… Compiled out | âŒ Always present | âœ… Compiled out | âœ… if constexpr |
| Expression capture | âŒ No | âŒ Manual | Partial | âœ… `#condition` + source_location |

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

| Scenario | Mechanism | Cost Driver |
|----------|-----------|-------------|
| Check enabled, passes | Comparison + predicted branch | Branch prediction strongly favors the passing path |
| Check enabled, fails | Raiser invocation | Exception construction (heap allocation for message) or `std::abort()` |
| Check disabled | Entire check compiled out via `if constexpr` | Zero — no code generated |

### Code Generation Comparison

```cpp
// assert (NDEBUG undefined)
assert(ptr != nullptr);
// â†’ if (!ptr) { __assert_fail(...); }

// Enforce (always enabled)
FATP_ALWAYS_ENFORCE(ptr != nullptr, "...");
// â†’ if (!ptr) { throw LogicContractError(...); }

// Enforce (debug-only, release build)
FATP_ENFORCE(ptr != nullptr, "...");
// â†’ (nothing - macro expands to ((void)0) under NDEBUG)
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
enforce_predicates.h     (NotNullPredicate, IsPositivePredicate, InRangePredicate, ...)
enforce_raisers.h        (LogicRaiser, AbortRaiser, WarningToCerrRaiser, ...)
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
