# ValueGuard: A Fat-P Library Showcase

## Executive Summary

ValueGuard is a policy-based RAII utility that temporarily mutates a variable and guarantees restoration upon scope exit—even when exceptions unwind the stack. Unlike manual save/restore patterns that scatter cleanup logic across code paths, ValueGuard encapsulates the mutation-restoration contract in a single object with compile-time policy selection. This architectural choice transforms error-prone manual bookkeeping into deterministic, exception-safe state management with zero virtual dispatch overhead.

---

## The Problem Domain

### What Goes Wrong Without It

```cpp
// The manual approach: fragile, verbose, exception-unsafe
void process_with_debug_mode(Config& config) {
    bool original_debug = config.debug_mode;
    config.debug_mode = true;
    
    try {
        do_work();           // Might throw
        do_more_work();      // Might throw
        config.debug_mode = original_debug;  // Easy to forget
    } catch (...) {
        config.debug_mode = original_debug;  // Duplicated cleanup
        throw;
    }
}

// Even worse: early returns create restoration landmines
bool validate_with_flag(Validator& v) {
    bool original = v.strict_mode;
    v.strict_mode = true;
    
    if (!check_phase_one()) {
        v.strict_mode = original;  // Must remember here
        return false;
    }
    if (!check_phase_two()) {
        v.strict_mode = original;  // And here
        return false;
    }
    v.strict_mode = original;      // And here
    return true;
}
```

| Issue | HPC Impact |
|-------|------------|
| Duplicated restoration logic | Code bloat; maintenance burden in hot paths |
| Exception-unsafe patterns | Silent state corruption during stack unwinding |
| Early-return restoration bugs | Difficult-to-diagnose intermittent failures |
| Manual bookkeeping overhead | Developer cognitive load; review burden |

### The Standard's Limitation

C++ provides no standard mechanism for scoped value mutation. The closest patterns are:

- **RAII wrappers for resources** (`std::unique_ptr`, `std::lock_guard`)—but these manage ownership, not temporary value changes
- **Scope exit utilities** (`std::experimental::scope_exit`)—but these require manual lambda construction for each restoration

Neither provides the policy-based restoration control that HPC workloads demand: conditional restoration based on success/failure, custom restoration logic for complex types, or move-only type support.

---

## Architecture: Policy-Based Restoration

```cpp
template <typename T, typename Policy = ValueGuardCopyPolicy<T>>
class [[nodiscard]] ValueGuard : private Policy {
    T* target_;
    T original_;
    bool active_;
    
    ~ValueGuard() noexcept(Policy::is_nothrow_restore) {
        if (active_) {
            this->execute(*target_, std::move(original_));
        }
    }
};
```

**The Mechanism:** ValueGuard inherits privately from its Policy, enabling Empty Base Optimization when policies are stateless. The destructor delegates restoration to `Policy::execute()`, which is resolved at compile time—no virtual dispatch, no branch misprediction from polymorphic calls. The `[[nodiscard]]` attribute prevents accidental immediate destruction of temporaries.

### Policy Resolution at Compile Time

```
┌─────────────────────────────────────────────────────────────┐
│                      ValueGuard<T, Policy>                  │
├─────────────────────────────────────────────────────────────┤
│  Policy selected at compile time via:                       │
│  • Explicit template argument                               │
│  • CTAD deduction guides                                    │
│  • Factory functions                                        │
├─────────────────────────────────────────────────────────────┤
│                    Available Policies                       │
├──────────────┬──────────────────────────────────────────────┤
│ CopyPolicy   │ target = original (copy-assign)              │
│ MovePolicy   │ target = std::move(original)                 │
│ NoRestore    │ No-op (commit the mutation)                  │
│ Conditional  │ Restore only if condition() returns true     │
│ Custom       │ User-provided restorer(target, original)     │
└──────────────┴──────────────────────────────────────────────┘
```

---

## Feature Inventory

### 1. Automatic Scope-Based Restoration

The core value proposition: mutation is undone when the guard leaves scope, regardless of control flow.

```cpp
// Without ValueGuard: 4 restoration points, 3 bugs waiting to happen
void fragile_function(State& state) {
    int original = state.mode;
    state.mode = PROCESSING;
    
    if (early_exit_condition()) {
        state.mode = original;  // Point 1
        return;
    }
    try {
        risky_operation();
    } catch (...) {
        state.mode = original;  // Point 2
        throw;
    }
    state.mode = original;      // Point 3
}

// With ValueGuard: 1 declaration, 0 restoration points, 0 bugs
void robust_function(State& state) {
    fat_p::ValueGuard guard(state.mode, PROCESSING);
    
    if (early_exit_condition()) return;  // Restored automatically
    risky_operation();                    // Exception? Restored automatically
}  // Normal exit? Restored automatically
```

### 2. Policy-Driven Restoration Behavior

Compile-time policy selection eliminates runtime branching in the destructor.

```cpp
// CopyPolicy (default for lvalue new_value): preserves original for inspection
int config_value = 100;
{
    fat_p::ValueGuard guard(config_value, 200);  // Deduces CopyPolicy
    assert(guard.original() == 100);              // Original accessible
}
assert(config_value == 100);  // Restored via copy

// MovePolicy (default for rvalue new_value): optimal for move-only types
std::unique_ptr<Resource> resource = std::make_unique<Resource>();
{
    fat_p::ValueGuard guard(resource, std::make_unique<Resource>());  // Deduces MovePolicy
}
// Original resource restored via move
```

### 3. Conditional Restoration

Decide at scope exit whether to restore—without branching in every code path.

```cpp
bool operation_succeeded = false;

{
    auto guard = fat_p::make_value_guard_conditional(
        database.transaction_id,
        generate_new_id(),
        [&]() { return !operation_succeeded; }  // Restore only on failure
    );
    
    operation_succeeded = perform_transaction();
    // If succeeded: guard releases, new ID kept
    // If failed: guard restores original ID
}
```

### 4. Custom Restoration Logic

Inject arbitrary restoration behavior for complex types or logging requirements.

```cpp
{
    auto guard = fat_p::make_value_guard_custom(
        counter,
        counter + 10,
        [](int& target, int&& original) {
            log("Restoring counter from {} to {}", target, original);
            target = original;
        }
    );
    // Custom restorer invoked on scope exit
}
```

### 5. Early Release (Commit Semantics)

Explicitly keep the mutation by disabling restoration.

```cpp
{
    fat_p::ValueGuard guard(config.mode, NEW_MODE);
    
    if (validate_new_mode()) {
        guard.release();  // Mutation committed; destructor becomes no-op
        return;
    }
    // Validation failed: destructor restores original mode
}
```

### 6. State Introspection

Access both original and current values during the guarded scope.

```cpp
fat_p::ValueGuard guard(value, 42);

assert(guard.original() == old_value);  // What it was
assert(guard.current() == 42);          // What it is now
assert(guard.is_active());              // Will restore on destruction
```

### 7. Move-Only Type Support

Full support for `std::unique_ptr`, file handles, and other non-copyable resources.

```cpp
std::unique_ptr<Connection> conn = open_connection();
{
    fat_p::ValueGuard<std::unique_ptr<Connection>, 
                      fat_p::ValueGuardMovePolicy<std::unique_ptr<Connection>>>
        guard(conn, open_temporary_connection());
    
    use_temporary_connection(*conn);
}
// Original connection restored via move
```

### 8. Strong Exception Guarantee

Constructor failure leaves the target unchanged.

```cpp
struct ThrowingType {
    ThrowingType& operator=(ThrowingType&&) { throw std::runtime_error("fail"); }
};

ThrowingType target;
try {
    fat_p::ValueGuard guard(target, ThrowingType{});  // Throws during assignment
} catch (...) {
    // target is in its original state, not moved-from
}
```

---

## Why Not Alternatives?

| If You Need... | Why Not Manual Save/Restore | Fat-P Advantage |
|----------------|----------------------------|-----------------|
| Exception safety | Requires try-catch around every path | Automatic via destructor |
| Multiple exit points | Each return needs restoration code | Single declaration |
| Move-only types | Manual pattern can't handle moves cleanly | Policy-based move support |
| Conditional restore | Branches scattered throughout function | Single lambda at construction |

| If You Need... | Why Not `scope_exit` | Fat-P Advantage |
|----------------|---------------------|-----------------|
| Original value access | Must capture explicitly in lambda | Built-in `original()` accessor |
| Compile-time policy | Lambda prevents optimization | Policy resolved at compile time |
| Type safety | Lambda captures can dangle | Pointer-based, no capture issues |
| Release semantics | Must add boolean flag manually | Built-in `release()` method |

| If You Need... | Why Not Custom RAII Class | Fat-P Advantage |
|----------------|--------------------------|-----------------|
| Reusability | One-off class per use case | Generic for any type |
| Policy flexibility | Hardcoded behavior | 5 built-in policies + custom |
| Deduction guides | Must write manually | C++17 CTAD support included |

---

## The "Forever Stuck" Reality

> **Compiler Reality Check:** Many HPC environments run RHEL 7/8 with GCC 7.x or 8.x due to driver compatibility (CUDA, InfiniBand). Even when C++23 offers `std::scope_exit`, your codebase may be contractually locked to C++17 for years.
>
> ValueGuard isn't waiting for a standard feature—it provides **policy-based restoration** that no standard proposal addresses. The compile-time policy resolution and move-only type support will remain valuable even after compiler upgrades.

---

## Performance Characteristics

| Operation | Complexity | Mechanism |
|-----------|------------|-----------|
| Construction | O(1) + assignment cost | Single pointer store + value capture |
| Destruction | O(1) + policy execution | Conditional restoration via policy |
| `release()` | O(1) | Boolean flag set |
| `original()`/`current()` | O(1) | Direct member/pointer access |
| `swap()` | O(1) + swap cost of T | Member-wise swap |

### Where Fat-P Wins

- **Recursive algorithms:** Stack of guards manages nested state changes automatically
- **Exception-heavy code:** Zero manual try-catch blocks for state restoration
- **Multi-exit functions:** Single declaration vs. N restoration points
- **Move-only resources:** Clean ownership transfer with automatic rollback

### Where Fat-P Loses (Honesty Builds Trust)

- **Trivial scopes:** For a single assignment with no exceptions and one exit point, manual save/restore has lower conceptual overhead
- **Very hot inner loops:** The guard object occupies stack space; for tight loops, hoist the guard outside
- **Non-assignable types:** ValueGuard requires `operator=` on the target type

---

## Integration Points

```
ValueGuard
    ↓ uses
FatPTypeTraits.h (is_value_guard trait)
    ↓ integrates with
ScopeGuard.h (complementary RAII patterns)
Expected.h (conditional restoration on error)
Signal.h (temporary slot state during emission)
```

**Common Pattern:** Combine with `Expected` for transactional semantics:

```cpp
fat_p::Expected<Result, Error> transactional_update(State& state) {
    auto guard = fat_p::make_value_guard_conditional(
        state.value,
        compute_new_value(),
        [&]() { return result.has_error(); }  // Restore on error
    );
    
    auto result = perform_update();
    return result;
}
```

---

## Final Assessment

ValueGuard delivers on the fat_p promise:

1. **Permanence:** Policy-based restoration is an architectural feature, not a stopgap. No C++ standard proposal offers compile-time policy selection for value restoration.

2. **Specialization:** Zero virtual dispatch, EBO for stateless policies, and move-only type support address HPC requirements that general-purpose scope-exit utilities ignore.

3. **Control:** Five built-in policies plus custom lambda support provide fine-grained control over restoration behavior—decided at compile time, not runtime.

**Architectural Verdict:** ValueGuard transforms the error-prone manual save/restore anti-pattern into a single-declaration, exception-safe, policy-customizable abstraction with zero runtime overhead.

---

*ValueGuard.h — Fat-P Library v2.4*
