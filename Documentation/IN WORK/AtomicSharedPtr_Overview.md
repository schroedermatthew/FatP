# AtomicSharedPtr: A Fat-P Library Showcase

## Executive Summary

AtomicSharedPtr is a **minimal thread-safe wrapper** for `std::shared_ptr` that provides atomic load, store, exchange, and compare-and-swap operations. Unlike bloated alternatives with policy explosions and mutex-based weak_ptr emulation, AtomicSharedPtr delivers **196 lines of focused functionality**—wrapping `std::atomic<std::shared_ptr<T>>` on C++20 or the free atomic functions on C++17. The single `ThrowOnNull` template parameter replaces complex enforcement policy hierarchies, transforming what was a 1,900-line abstraction into a component that does exactly what users need and nothing more.

---

## The Problem Domain

### What Goes Wrong Without It

```cpp
// C++17: The free function API is awkward
std::shared_ptr<Config> global_config;

void update_config(std::shared_ptr<Config> new_config)
{
    // Verbose, easy to forget the _explicit suffix
    std::atomic_store_explicit(&global_config, new_config, std::memory_order_release);
}

std::shared_ptr<Config> get_config()
{
    // Every call site needs to remember this pattern
    return std::atomic_load_explicit(&global_config, std::memory_order_acquire);
}

// C++20: Better, but still raw
std::atomic<std::shared_ptr<Config>> global_config;
// Users must understand memory ordering at every call site
```

| Issue | HPC Impact |
|-------|------------|
| Verbose free function syntax | Error-prone boilerplate at every access |
| Memory ordering exposed | Wrong ordering causes subtle data races |
| No null-safety option | Crashes propagate to unrelated code |
| No C++17/C++20 abstraction | Code locked to specific standard version |

### The Standard's Limitation

C++20 introduced `std::atomic<std::shared_ptr<T>>`, but:

- **C++17 is stuck with free functions**—`std::atomic_load`, `std::atomic_store`, etc.
- **No abstraction over versions**—code must `#ifdef` between approaches
- **No null enforcement**—both versions return potentially-null pointers silently
- **Raw memory ordering**—every call site chooses orderings, inviting mistakes

---

## Architecture: Zero-Overhead Version Abstraction

```cpp
template <typename T, bool ThrowOnNull = false>
class AtomicSharedPtr
{
private:
#if FATP_HAS_CPP20_ATOMIC_SHARED_PTR
    std::atomic<std::shared_ptr<T>> ptr_;
#else
    std::shared_ptr<T> ptr_;  // Uses free atomic functions
#endif

public:
    std::shared_ptr<T> load(std::memory_order order = std::memory_order_acquire) const;
    void store(std::shared_ptr<T> p, std::memory_order order = std::memory_order_release) noexcept;
    std::shared_ptr<T> exchange(std::shared_ptr<T> p, std::memory_order order = std::memory_order_acq_rel) noexcept;
    bool compare_exchange_weak(std::shared_ptr<T>& expected, std::shared_ptr<T> desired, ...);
    bool compare_exchange_strong(std::shared_ptr<T>& expected, std::shared_ptr<T> desired, ...);
};
```

**The Mechanism:** Compile-time detection via `__cpp_lib_atomic_shared_ptr` (the standard library feature test macro) selects the optimal implementation. This is more accurate than checking `__cplusplus`—a compiler might support C++20 syntax but the standard library might not have `std::atomic<shared_ptr>` yet. On supported platforms, you get native `std::atomic<shared_ptr>` with potential lock-free operations. On C++17, the free atomic functions provide the same semantics with guaranteed correctness. The abstraction cost is zero—the wrapper inlines away completely.

**The Simplification:** A single boolean template parameter `ThrowOnNull` replaces four enforcement policies, three wait policies, a duration policy, and an invariant guard. Users who want null-checking get it; users who don't pay nothing for the option.

---

## Feature Inventory

### 1. Atomic Load with Optional Null Enforcement

```cpp
// Default: returns nullptr if empty (no overhead)
fat_p::AtomicSharedPtr<Config> config;
auto ptr = config.load();  // Returns nullptr, no exception

// ThrowOnNull: throws if empty (opt-in safety)
fat_p::AtomicSharedPtr<Config, true> safe_config;
auto ptr = safe_config.load();  // Throws std::runtime_error if null

// raw_load: never throws, even with ThrowOnNull
auto ptr = safe_config.raw_load();  // Returns nullptr without throwing
```

**Why two load functions:** `load()` respects the `ThrowOnNull` policy for code that wants enforcement. `raw_load()` provides an escape hatch for conditional checks without exception overhead.

### 2. Atomic Store, Exchange, and CAS

```cpp
fat_p::AtomicSharedPtr<Config> config;

// Store: replaces current value
config.store(std::make_shared<Config>(8080, "localhost"));

// Exchange: replaces and returns old value
auto old = config.exchange(std::make_shared<Config>(9090, "remote"));

// Compare-and-swap: conditional update
auto expected = config.load();
auto desired = std::make_shared<Config>(expected->port + 1, expected->host);
if (config.compare_exchange_strong(expected, desired))
{
    // Successfully incremented port atomically
}
```

**Zero overhead:** These operations compile to exactly what you'd write by hand—direct calls to `std::atomic` methods or free functions depending on C++ version.

### 3. Native Wait/Notify (C++20 Only)

```cpp
#if FATP_HAS_CPP20_ATOMIC_SHARED_PTR
fat_p::AtomicSharedPtr<Config> config(initial_config);

// Thread 1: Wait for config change
auto old = config.load();
config.wait(old);  // Blocks until config differs from old
auto new_config = config.load();
apply(new_config);

// Thread 2: Update and notify
config.store(updated_config);
config.notify_all();  // Wakes all waiting threads
#endif
```

**Why C++20 only:** Native `std::atomic::wait` provides OS-level efficiency that can't be portably replicated in C++17. Rather than ship a polling fallback that users might mistake for efficient, AtomicSharedPtr exposes wait/notify only where it performs well.

### 4. Lock-Free Detection

```cpp
fat_p::AtomicSharedPtr<Config> config;

// Runtime query
if (config.is_lock_free())
{
    // Lock-free on this platform (rare for shared_ptr)
}

// Compile-time query (for static_assert, template selection)
if constexpr (fat_p::AtomicSharedPtr<Config>::is_always_lock_free())
{
    // Guaranteed lock-free on this platform
}
```

**C++17:** Both functions return `false`—the free-function atomics have no lock-free query.

**C++20:** Queries the actual `std::atomic<shared_ptr>` implementation.

**Reality check:** Most `std::atomic<shared_ptr>` implementations are *not* lock-free due to control block manipulation. These functions tell you the truth about your platform.

### 5. Factory and Type Traits

```cpp
// Factory function
auto config = fat_p::make_atomic_shared_ptr<Config>(8080, "localhost");
auto safe_config = fat_p::make_atomic_shared_ptr<Config, true>(8080, "localhost");

// Type detection
static_assert(fat_p::is_atomic_shared_ptr_v<decltype(config)>);
static_assert(!fat_p::is_atomic_shared_ptr_v<std::shared_ptr<Config>>);
```

---

## Why Not Alternatives?

| If You Need... | Why Not std::atomic<shared_ptr> Directly | Why Not the Previous AtomicReference | Fat-P Advantage |
|----------------|------------------------------------------|--------------------------------------|-----------------|
| C++17 support | Doesn't exist in C++17 | ✓ Supported | ✓ Single API for both versions |
| Null enforcement | No built-in option | 4 policy templates to configure | ✓ Single bool parameter |
| Simple mental model | Raw memory ordering everywhere | 4 template parameters, 1,900 lines | ✓ 196 lines, 2 template params |
| Efficient wait/notify | ✓ Native on C++20 | Custom polling with ABA hacks | ✓ Native only, no fake fallback |
| weak_ptr support | N/A | Mutex-based emulation | ✗ Not offered (use mutex directly) |

**The key insight:** AtomicSharedPtr does *less* than the previous AtomicReference—and that's the feature. Weak_ptr "atomic" operations required a mutex, meaning the "Atomic" name was misleading. Custom wait policies reimplemented what `std::atomic::wait` already does better. ABA protection via `owner_before` was only needed because of the weak_ptr complexity. Removing these non-features removed the bugs.

---

## The "Forever Stuck" Reality

**Compiler Reality Check:** Many HPC environments run RHEL 7/8 with GCC 7.x–9.x, locked to C++17 for driver compatibility. Even organizations with C++20 compilers often maintain C++17 codebases for toolchain consistency.

AtomicSharedPtr provides:

- **Identical API on C++17 and C++20**—no `#ifdef` at call sites
- **Automatic upgrade path**—recompile with C++20 and get native `std::atomic<shared_ptr>` automatically
- **Permanent value**—even on C++20, the null-enforcement and simplified API justify the wrapper

This isn't a polyfill waiting for C++20. It's a better API than raw `std::atomic<shared_ptr>` regardless of language version.

---

## Performance Characteristics

| Operation | Complexity | Mechanism |
|-----------|------------|-----------|
| `load()` | O(1) | Atomic load + optional null check |
| `store()` | O(1) | Atomic store |
| `exchange()` | O(1) | Atomic exchange |
| `compare_exchange_*` | O(1) | Atomic CAS |
| `wait()` | OS-dependent | Native `std::atomic::wait` (C++20) |
| `is_lock_free()` | O(1) | Runtime platform query |
| `is_always_lock_free()` | O(0) | Compile-time constant |

### Where Fat-P Wins

- **Version abstraction**: Single codebase for C++17 and C++20
- **Null safety**: Optional compile-time enforcement without runtime cost when disabled
- **Simplicity**: 196 lines vs 1,900 lines means fewer bugs, easier auditing

### Where Fat-P Loses (Honesty Builds Trust)

- **No weak_ptr support**: If you need atomic weak_ptr operations, use `std::mutex` + `std::weak_ptr` directly—don't pretend it's lock-free
- **No custom wait policies**: If you need exotic wait behavior (polling with custom backoff), write it yourself for your specific use case
- **No wait on C++17**: `wait()` and `notify_*()` are C++20 only—no fake polling fallback

---

## Integration Points

```
AtomicSharedPtr.h
    ↓ used by
Signal.h            (thread-safe slot management)
ResourceCache.h     (concurrent cache updates)
ConfigManager.h     (hot-reloadable configuration)
    ↓ uses
<atomic>            (std::atomic, std::memory_order)
<memory>            (std::shared_ptr)
```

---

## Final Assessment

AtomicSharedPtr delivers on the fat_p promise through three pillars:

### 1. Permanence
This isn't waiting for C++20—it's a better API than raw `std::atomic<shared_ptr>` even when that's available. The null-enforcement option and version abstraction provide permanent value.

### 2. Specialization
HPC codebases need predictable behavior, low overhead, and simple mental models. AtomicSharedPtr provides exactly the atomic shared_ptr operations that users actually need, without the policy explosion that creates bugs.

### 3. Control
The `ThrowOnNull` parameter gives compile-time control over null behavior. `raw_load()` provides an escape hatch. Memory ordering parameters are available but default to safe values. Users control what they need to control.

**Architectural Verdict:** AtomicSharedPtr transforms atomic shared_ptr access from **verbose, version-specific incantations** into a **clean, portable, optionally-safe abstraction**—in 196 lines that compile to zero overhead.

---

*AtomicSharedPtr.h (196 lines) — Fat-P Library*
