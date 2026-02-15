# ConcurrencyPolicies: A Fat-P Library Showcase

## Executive Summary

ConcurrencyPolicies is a **compile-time threading strategy system** that provides zero-overhead single-threaded operation through Empty Base Optimization (EBO) while enabling mutex or shared-mutex synchronization via the same interface. Unlike runtime-checked thread safety (virtual dispatch overhead) or hardcoded threading choices (inflexible), ConcurrencyPolicies lets containers and algorithms accept threading strategy as a template parameter—generating optimal code for each case with **zero abstraction overhead**.

---

## The Problem Domain

### What Goes Wrong Without It

```cpp
// The "always lock" trap
template<typename T>
class Container {
    std::mutex mutex_;  // Overhead even for single-threaded use
    std::vector<T> data_;
    
public:
    void add(T value) {
        std::lock_guard lock(mutex_);  // ~20ns overhead EVERY call
        data_.push_back(std::move(value));
    }
};

// The "conditional lock" mess
template<typename T>
class ConditionalContainer {
    bool thread_safe_;
    std::mutex mutex_;
    std::vector<T> data_;
    
public:
    void add(T value) {
        if (thread_safe_) {
            std::lock_guard lock(mutex_);
            data_.push_back(std::move(value));
        } else {
            data_.push_back(std::move(value));
        }
    }
    // Branch on every operation. Duplicated code. Runtime overhead.
};
```

| Issue | HPC Impact |
|-------|------------|
| Always-lock overhead | 20-50ns per lock acquisition even in single-threaded code |
| Runtime conditionals | Branch prediction overhead on every operation |
| Code duplication | Two implementations for threaded/non-threaded |
| No reader-writer | Mutex blocks readers unnecessarily |

### The Standard's Limitation

The C++ standard provides `std::mutex` and `std::shared_mutex` but no abstraction for "optional" or "conditional" locking. Templates cannot easily switch between "no locking" and "locking" without code duplication or runtime checks.

**The fundamental problem:** How do you write a container that's optimal for single-threaded use but thread-safe when needed, without duplicating the entire implementation?

---

## Architecture: Policy-Based Locking with EBO

### The Mechanism: Empty Base Optimization

```cpp
// Single-threaded: zero storage, zero overhead
struct SingleThreadedPolicy {
    struct LockGuard { };  // Empty struct
    LockGuard lock() { return {}; }  // No-op, optimized away
    void unlock() { }
};

// Mutex-based: real synchronization
struct MutexSynchronizationPolicy {
    mutable std::mutex mutex_;
    std::unique_lock<std::mutex> lock() { 
        return std::unique_lock(mutex_); 
    }
};

// Shared-mutex: reader-writer locking
struct SharedMutexPolicy {
    mutable std::shared_mutex mutex_;
    std::unique_lock<std::shared_mutex> lock() { 
        return std::unique_lock(mutex_); 
    }
    std::shared_lock<std::shared_mutex> lock_shared() { 
        return std::shared_lock(mutex_); 
    }
};
```

**Why EBO matters:**

```cpp
// Container inherits from policy
template<typename T, typename Policy = SingleThreadedPolicy>
class Container : private Policy {  // EBO-eligible
    std::vector<T> data_;
public:
    void add(T value) {
        auto guard = this->lock();  // Policy provides lock()
        data_.push_back(std::move(value));
    }
};

// For SingleThreadedPolicy:
// - sizeof(Container<int, SingleThreadedPolicy>) == sizeof(vector<int>)
// - lock() returns empty struct → optimized to nothing
// - Zero overhead vs. non-policy version
```

### Compile-Time Selection

```cpp
// Single-threaded context: zero overhead
Container<int, SingleThreadedPolicy> st_container;
st_container.add(42);  // No locking code generated

// Multi-threaded context: proper synchronization
Container<int, MutexSynchronizationPolicy> mt_container;
mt_container.add(42);  // std::mutex lock/unlock

// Read-heavy multi-threaded: shared locks for readers
Container<int, SharedMutexPolicy> shared_container;
// Writers: exclusive lock
// Readers: shared lock (concurrent reads allowed)
```

---

## Feature Inventory

### 1. SingleThreadedPolicy: Zero Overhead

```cpp
struct SingleThreadedPolicy {
    struct LockGuard { 
        explicit LockGuard() = default;
    };
    
    [[nodiscard]] LockGuard lock() const noexcept { return LockGuard{}; }
    [[nodiscard]] LockGuard lock_shared() const noexcept { return LockGuard{}; }
    void unlock() const noexcept { }
    void unlock_shared() const noexcept { }
};
```

**Generated code:** The empty `LockGuard` is optimized away. `lock()` compiles to nothing. Containers using `SingleThreadedPolicy` have **identical codegen** to non-thread-aware containers.

### 2. MutexSynchronizationPolicy: Exclusive Locking

```cpp
struct MutexSynchronizationPolicy {
    mutable std::mutex mutex_;
    
    [[nodiscard]] std::unique_lock<std::mutex> lock() const {
        return std::unique_lock<std::mutex>(mutex_);
    }
    
    [[nodiscard]] std::unique_lock<std::mutex> lock_shared() const {
        return std::unique_lock<std::mutex>(mutex_);  // No shared mode
    }
};
```

**Use case:** Write-heavy workloads where reader-writer distinction doesn't help.

### 3. SharedMutexPolicy: Reader-Writer Locking

```cpp
struct SharedMutexPolicy {
    mutable std::shared_mutex mutex_;
    
    [[nodiscard]] std::unique_lock<std::shared_mutex> lock() const {
        return std::unique_lock<std::shared_mutex>(mutex_);
    }
    
    [[nodiscard]] std::shared_lock<std::shared_mutex> lock_shared() const {
        return std::shared_lock<std::shared_mutex>(mutex_);
    }
};
```

**Use case:** Read-heavy workloads—multiple readers can proceed concurrently; only writers need exclusive access.

### 4. SpinLockPolicy: Low-Latency Locking

```cpp
struct SpinLockPolicy {
    mutable std::atomic_flag flag_ = ATOMIC_FLAG_INIT;
    
    class LockGuard {
        std::atomic_flag& flag_;
    public:
        explicit LockGuard(std::atomic_flag& f) : flag_(f) {
            while (flag_.test_and_set(std::memory_order_acquire)) {
                // Spin
            }
        }
        ~LockGuard() { flag_.clear(std::memory_order_release); }
    };
    
    [[nodiscard]] LockGuard lock() const { return LockGuard(flag_); }
};
```

**Use case:** Very short critical sections where mutex overhead exceeds wait time.

### 5. Policy Traits for Generic Code

```cpp
template<typename Policy>
struct PolicyTraits {
    static constexpr bool is_thread_safe = 
        !std::is_same_v<Policy, SingleThreadedPolicy>;
    
    static constexpr bool supports_shared_lock = 
        std::is_same_v<Policy, SharedMutexPolicy>;
};

// Usage in generic code
template<typename Policy>
void optimize_for_policy() {
    if constexpr (PolicyTraits<Policy>::supports_shared_lock) {
        // Use shared locks for reads
    } else {
        // Use exclusive locks
    }
}
```

---

## Why Not Alternatives?

| If You Need... | Why Not Always Mutex | Why Not Runtime Flag | Why Not Separate Classes | Fat-P Advantage |
|----------------|---------------------|---------------------|------------------------|-----------------|
| Zero single-threaded overhead | ❌ 20-50ns per lock | ❌ Branch overhead | ✅ Zero | ✅ EBO to zero |
| One codebase | ✅ One class | ✅ One class | ❌ Duplicate code | ✅ One template |
| Reader-writer option | ❌ Mutex only | Manual | Manual | ✅ SharedMutexPolicy |
| Compile-time selection | ❌ Runtime | ❌ Runtime | ✅ Compile-time | ✅ Compile-time |

**The Sweet Spot:** ConcurrencyPolicies is the only option combining:
- ✅ Zero overhead for single-threaded (EBO)
- ✅ Single codebase (template parameter)
- ✅ Multiple locking strategies (mutex, shared, spin)
- ✅ Compile-time policy resolution

---

## The "Forever Stuck" Reality

**Standard Library Reality:** The committee will not provide policy-based locking because:

1. **ABI concerns:** Adding policy templates would break binary compatibility
2. **Complexity:** Standard containers prioritize simplicity over customization
3. **No consensus:** Different threading models (mutex, spinlock, RCU) suit different workloads

Fat-p components use ConcurrencyPolicies internally (StringPool, IdGenerator, Signal), demonstrating how to build thread-safety into data structures without hardcoding a single strategy.

---

## Performance Characteristics

### Performance Characteristics

| Policy | Lock Mechanism | Memory Overhead |
|--------|---------------|-----------------|
| SingleThreadedPolicy | No synchronization — compiles to nothing | 0 bytes |
| MutexSynchronizationPolicy | OS mutex lock/unlock | 40 bytes (mutex) |
| SharedMutexPolicy | OS shared mutex — shared lock for reads, exclusive for writes | 56 bytes |
| SpinLockPolicy | Atomic flag CAS spin — no OS transition when uncontended | 1 byte (atomic_flag) |

See `components/ConcurrencyPolicies/results/` for current platform-specific benchmark data.

### Where Fat-P Wins

- **Library code:** Write once, support both single and multi-threaded use
- **Configurable containers:** Users choose threading at instantiation
- **Performance-critical paths:** SingleThreadedPolicy guarantees zero overhead

### Where Fat-P Loses (Honesty Builds Trust)

- **Dynamic threading needs:** If thread safety must change at runtime, use runtime checks
- **Very complex locking:** For priority inheritance or RCU, use specialized libraries
- **Cross-process:** These policies are in-process only; for shared memory, use OS primitives

---

## Integration Points

```
ConcurrencyPolicies.h
    ↓ used by
StringPool.h        (policy-based thread-safe interning)
IdGenerator.h       (policy-based thread-safe ID generation)
Signal.h            (policy-based thread-safe signals)
ObjectPool.h        (policy-based thread-safe pools)
```

---

## Final Assessment

ConcurrencyPolicies delivers on the fat_p promise through three pillars:

### 1. Permanence
The standard library cannot add policy-based threading to existing containers (ABI break). ConcurrencyPolicies provides the pattern for fat_p components and user code, permanently enabling the "write once, configure threading" pattern.

### 2. Specialization
Empty Base Optimization ensures zero overhead for `SingleThreadedPolicy`. Reader-writer locks (`SharedMutexPolicy`) enable concurrent reads. Spinlocks minimize latency for short critical sections. Each policy optimizes for its use case.

### 3. Control
Threading strategy is a template parameter—visible in the type, resolved at compile time. No runtime checks, no virtual dispatch. The policy IS the threading model.

**Architectural Verdict:** ConcurrencyPolicies transforms thread-safety from **all-or-nothing** (always mutex or no locking) to **compile-time configurable** (SingleThreaded, Mutex, SharedMutex, SpinLock). It's the infrastructure that makes fat_p components thread-safe without sacrificing single-threaded performance.

---

*ConcurrencyPolicies.h — Fat-P Library*
