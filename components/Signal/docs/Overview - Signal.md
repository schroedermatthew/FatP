# Signal: A Fat-P Library Showcase

## Executive Summary

Signal is a **policy-based signal/slot implementation** with SmallVector-backed storage that achieves **zero heap allocation for ≤4 listeners** while providing thread-safety, exception handling, and emission control through compile-time policy selection. Unlike Qt signals (requires MOC, runtime overhead) or Boost.Signals2 (complex, heavyweight), fat_p Signal composes storage, threading, and exception policies into exactly the behavior you need—with the common case of few listeners optimized to avoid allocation entirely.

---

## The Problem Domain

### What Goes Wrong Without It

```cpp
// The callback spaghetti
class Button {
    std::vector<std::function<void()>> callbacks_;
    
public:
    void onClick(std::function<void()> cb) {
        callbacks_.push_back(std::move(cb));  // Heap allocation
    }
    
    void click() {
        for (auto& cb : callbacks_) {
            cb();  // What if cb throws?
            // What if cb modifies callbacks_ during iteration?
            // What if cb disconnects itself?
        }
    }
    
    void disconnect(/* which one? */) {
        // No way to identify callbacks!
    }
};
```

| Issue | HPC Impact |
|-------|------------|
| Heap allocation per listener | Allocation overhead for common small-listener-count case |
| No disconnect mechanism | Can't remove specific listeners |
| Reentrancy hazards | Modifying listeners during emission corrupts iteration |
| No exception handling | One throwing listener breaks all subsequent listeners |
| No thread safety | Data races when connecting/emitting from different threads |

### The Standard's Limitation

The C++ standard provides no signal/slot mechanism. `std::function` handles single callbacks but not:
- Multiple listeners
- Disconnect by handle
- Reentrancy-safe iteration
- Exception handling policies
- Thread safety options

---

## Architecture: SmallVector Storage with Policy Composition

### The Mechanism: Inline Storage for Common Case

```cpp
template<typename Signature,
         typename SyncPolicy = SingleThreadedPolicy,
         typename EmissionPolicy = CatchAndIgnorePolicy,
         size_t InlineCapacity = 4>
class Signal;

// SmallVector stores ≤InlineCapacity slots inline (zero allocation)
using SlotList = SmallVector<Slot, InlineCapacity>;

// Slots stored as:
struct Slot {
    ConnectionId id;           // Type-safe connection identifier (StrongId)
    Callback func;             // std::function-based callback
    int priority = 0;          // Ordering (higher = called first)
    std::atomic<bool> active;  // Soft-delete flag for reentrancy-safe removal
};
```

**Why SmallVector matters:**

Most signals have 1-4 listeners. SmallVector stores these inline:
- **0 listeners:** No allocation
- **1-4 listeners:** Stack storage only
- **5+ listeners:** Automatic heap promotion

### Thread Safety Policies

```cpp
Signal<void()>                             sig1;  // Single-threaded (zero overhead)
Signal<void(), MutexSynchronizationPolicy> sig2;  // Mutex-protected
Signal<void(), SharedMutexPolicy>          sig3;  // Reader-writer
```

### Exception Policies

```cpp
// CatchAndIgnorePolicy: Swallow exception, continue to next slot (default)
// PropagateExceptionPolicy: First exception propagates; later slots skipped
// TerminateOnExceptionPolicy: std::terminate on any exception (noexcept emission)
```

---

## Feature Inventory

### 1. Zero-Allocation Common Case

```cpp
Signal<void(int)> valueChanged;

// First 4 connections: zero heap allocation
auto c1 = valueChanged.connect([](int v) { handle1(v); });
auto c2 = valueChanged.connect([](int v) { handle2(v); });
auto c3 = valueChanged.connect([](int v) { handle3(v); });
auto c4 = valueChanged.connect([](int v) { handle4(v); });

// SmallVector stores all 4 inline
// Total heap allocations: 0
```

### 2. RAII Connection Management

```cpp
class Observer {
    ScopedConnection connection_;
    
public:
    void observe(Signal<void(int)>& sig) {
        connection_ = sig.connect([this](int v) { onValue(v); });
    }
    
    ~Observer() {
        // connection_ automatically disconnects
    }
};
```

**Mechanism:** `ScopedConnection` stores signal reference and connection ID. Destructor calls `disconnect()`.

**Note:** There is no slot block/unblock API. Disconnection—manual `disconnect(id)` or `ScopedConnection` going out of scope—is the mechanism for stopping delivery to a slot, temporarily or permanently.

### 3. Reentrancy-Safe Emission

```cpp
Signal<void()> sig;

auto c1 = sig.connect([&] {
    sig.disconnect(c1);  // Disconnect self during emission
});

auto c2 = sig.connect([&] {
    sig.connect([] { /* new listener */ });  // Add during emission
});

sig.emit();  // Safe! Uses deferred-sweep algorithm
```

**Mechanism:** Emission iterates a snapshot or uses deferred removal. Modifications during emission are queued and applied after emission completes.

### 4. Exception Handling Policies

```cpp
// Policy 1: Propagate first exception
Signal<void(), SingleThreadedPolicy, PropagateExceptionPolicy> sig1;
sig1.connect([] { throw std::runtime_error("!"); });
sig1.connect([] { /* never called */ });
sig1.emit();  // Throws, second slot skipped

// Policy 2: Catch and continue (default)
Signal<void(), SingleThreadedPolicy, CatchAndIgnorePolicy> sig2;
sig2.connect([] { throw std::runtime_error("!"); });
sig2.connect([] { /* still called */ });
sig2.emit();  // All slots called, exceptions swallowed

// Policy 3: Terminate on exception (for noexcept emission contexts)
Signal<void(), SingleThreadedPolicy, TerminateOnExceptionPolicy> sig3;
// A throwing slot prints a diagnostic to stderr, then std::terminate()
```

### 5. Priority-Based Slot Ordering

```cpp
Signal<void()> sig;

sig.connect([] { log("Priority 0"); });           // Default priority: 0
sig.connect([] { log("Priority 10"); }, 10);      // Higher priority
sig.connect([] { log("Priority -5"); }, -5);      // Lower priority

sig.emit();
// Output order: "Priority 10", "Priority 0", "Priority -5"
```

### 6. Result Collection

```cpp
Signal<int()> compute;
compute.connect([] { return 1; });
compute.connect([] { return 2; });
compute.connect([] { return 3; });

// Collect all results
std::vector<int> results = compute.emitCollect();  // {1, 2, 3}

// Stop on condition
auto first_positive = compute.emitUntil([](int r) { return r > 0; });
```

---

## Why Not Alternatives?

| If You Need... | Why Not std::function | Why Not Qt Signals | Why Not Boost.Signals2 | Fat-P Advantage |
|----------------|----------------------|-------------------|----------------------|-----------------|
| Multiple listeners | ❌ Single only | ✅ Multiple | ✅ Multiple | ✅ Multiple |
| Zero allocation (small) | ❌ Always heap | ❌ Always heap | ❌ Always heap | ✅ SmallVector |
| No MOC/preprocessing | ✅ Works | ❌ Requires MOC | ✅ Works | ✅ Works |
| Thread safety options | ❌ None | ❌ Fixed | ✅ Options | ✅ Policy-based |
| Zero dependencies | ✅ Standard | ❌ Requires Qt | ❌ Requires Boost | ✅ Single header |
| Exception policies | ❌ None | ❌ Fixed | Limited | ✅ Three policies |

**The Sweet Spot:** Signal is the only option combining SmallVector storage, policy-based threading/exceptions, RAII connections, and zero external dependencies.

---

## The "Forever Stuck" Reality

**Standard Reality:** C++ will not standardize signal/slot:
- Design space is too large (threading, ownership, return values)
- Qt/Boost already dominate, no consensus on alternative
- Signals are often domain-specific (UI, games, embedded)

Fat_p Signal provides a production-ready implementation that:
- Works without MOC or code generation
- Has zero external dependencies
- Optimizes for the common small-listener case

---

## Performance Characteristics

| Scenario | Mechanism | Cost Driver |
|----------|-----------|-------------|
| Connect (≤4 slots) | SmallVector inline `push_back` | No heap allocation; pointer assignment only |
| Connect (>4 slots) | SmallVector heap-backed `push_back` | Single heap allocation on transition from inline to heap |
| Emit (N slots) | Direct iteration over slot storage | N × callback cost; contiguous for ≤4 slots |
| Disconnect | ID lookup + removal from slot vector | O(n) scan in slot count |
| Emit with mutex | SharedMutexPolicy shared lock + emit | Lock acquisition overhead added per emit |

See `components/Signal/results/` for current platform-specific benchmark data.

### Where Fat-P Wins
- Event-driven systems with many small signals (1-4 listeners typical)
- Games/simulations with frequent signal emission
- Libraries needing signal/slot without Qt/Boost dependency

### Where Fat-P Loses (Honesty Builds Trust)
- Deep Qt integration → Qt signals integrate better with Qt ecosystem
- Signals with 100+ listeners → specialized broadcast mechanisms may be better
- Single callback only → `std::function` is simpler

---

## Integration Points

```
Signal.h
    ↓ uses
SmallVector.h          (inline storage for ≤4 slots)
ConcurrencyPolicies.h  (threading policies)
    ↓ used by
UI event systems
Game entity components
Observer pattern implementations
```

---

## Final Assessment

Signal delivers on the fat_p promise through three pillars:

### 1. Permanence
C++ will never standardize signal/slot—too domain-specific, too many design choices. Signal provides a production-ready implementation permanently, without Qt/Boost dependency.

### 2. Specialization
SmallVector storage optimizes for the common case: most signals have 1-4 listeners. Zero heap allocation for this case transforms allocation-bound event systems into compute-bound.

### 3. Control
Four axes of customization (storage capacity, threading, exceptions, priority) let you configure exactly the behavior you need. Single-threaded signals have zero synchronization overhead. Exception policies match your error handling strategy.

**Architectural Verdict:** Signal transforms event handling from **heap-heavy callback lists** to **inline-optimized, policy-configured** signal/slot with RAII connection management. It's Qt signals without Qt.

---

*Signal.h (~1160 lines) — Fat-P Library*
