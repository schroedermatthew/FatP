# ConcurrencyPolicies User Manual

**Version:** 4.1  
**Library:** fat_p C++ Utilities  
**Standard:** C++17 (C++20/23 enhanced)  
**Type:** Header-only

---

## Table of Contents

1. [Overview](#overview)
2. [Quick Start](#quick-start)
3. [Policy Categories](#policy-categories)
4. [Basic Policies](#basic-policies)
5. [Advanced Policies](#advanced-policies)
6. [Policy Traits](#policy-traits)
7. [Usage Patterns](#usage-patterns)
8. [Performance Guide](#performance-guide)
9. [Best Practices](#best-practices)

---

## Overview

ConcurrencyPolicies provides 19 synchronization policies for policy-based design, ranging from zero-cost single-threaded to advanced lock-free primitives.

### Include

```cpp
#include "ConcurrencyPolicies.h"
using namespace fat_p;
```

### Design Philosophy

- **Policy-based**: Inject synchronization behavior via template parameters
- **Zero-overhead principle**: Single-threaded policy has no runtime cost
- **Composable**: Policies share a common interface
- **Production-ready**: Battle-tested implementations

---

## Quick Start

### Basic Usage

```cpp
template <typename SyncPolicy = SingleThreadedPolicy>
class Counter : private SyncPolicy {
    int value_ = 0;
public:
    void increment() {
        auto lock = this->lock();  // Policy-dependent
        ++value_;
    }
    
    int get() const {
        auto lock = this->lock();
        return value_;
    }
};

// Single-threaded (zero overhead)
Counter<SingleThreadedPolicy> st_counter;

// Thread-safe with mutex
Counter<MutexSynchronizationPolicy> mt_counter;

// Lock-free (where applicable)
Counter<SpinlockSynchronizationPolicy> spin_counter;
```

### Selecting a Policy

| Use Case | Recommended Policy |
|----------|-------------------|
| Single-threaded code | `SingleThreadedPolicy` |
| General thread safety | `MutexSynchronizationPolicy` |
| Read-heavy workloads | `SharedMutexPolicy` |
| Low contention | `SpinlockSynchronizationPolicy` |
| Real-time systems | `TicketLockPolicy` |
| Lock-free reads | `RCUPolicy` |
| High scalability | `MCSLockPolicy` |

---

## Policy Categories

### 1. No-Op Policies
- `SingleThreadedPolicy` - Zero overhead
- `LockFreeSynchronizationPolicy` - Debug assertions only

### 2. Mutex-Based Policies
- `MutexSynchronizationPolicy` - Standard mutex
- `RecursiveMutexPolicy` - Recursive locking
- `TimedMutexPolicy` - Timed lock attempts
- `SharedMutexPolicy` - Read-write lock
- `SharedTimedMutexPolicy` - Timed read-write

### 3. Spinlock Policies
- `SpinlockSynchronizationPolicy` - Busy-wait
- `TicketLockPolicy` - FIFO fair spinlock
- `MCSLockPolicy` - NUMA-scalable queue lock
- `AdaptiveLockPolicy` - Spin then sleep

### 4. Lock-Free Policies
- `RCUPolicy` - Read-Copy-Update
- `HazardPointerPolicy` - Safe memory reclamation
- `SeqLockPolicy` - Optimistic readers
- `VersionedLockPolicy` - MVCC-style

### 5. Specialized Policies
- `PriorityInheritanceLockPolicy` - Priority inversion prevention
- `WaitableSynchronizationPolicy` - Condition variables

---

## Basic Policies

### SingleThreadedPolicy

Zero-overhead policy for single-threaded contexts.

```cpp
struct SingleThreadedPolicy {
    struct Lock { };  // Empty RAII type
    Lock lock() const noexcept { return {}; }
    Lock lock_shared() const noexcept { return {}; }
};

// All methods compile to nothing
template <typename T>
class Container : private SingleThreadedPolicy {
    // lock() calls are optimized away completely
};
```

**Use when**: Single-threaded or external synchronization exists.

### MutexSynchronizationPolicy

Standard mutex-based synchronization.

```cpp
template <typename T>
class SafeQueue : private MutexSynchronizationPolicy {
    std::queue<T> queue_;
public:
    void push(T value) {
        auto lock = this->lock();  // RAII lock_guard
        queue_.push(std::move(value));
    }
    
    std::optional<T> pop() {
        auto lock = this->lock();
        if (queue_.empty()) return std::nullopt;
        T value = std::move(queue_.front());
        queue_.pop();
        return value;
    }
};
```

**Performance**: ~20-50ns per lock/unlock (uncontended)

### SharedMutexPolicy

Read-write lock for read-heavy workloads.

```cpp
template <typename T>
class Cache : private SharedMutexPolicy {
    std::map<std::string, T> data_;
public:
    std::optional<T> get(const std::string& key) const {
        auto lock = this->lock_shared();  // Multiple readers OK
        auto it = data_.find(key);
        return it != data_.end() ? std::optional(it->second) : std::nullopt;
    }
    
    void set(const std::string& key, T value) {
        auto lock = this->lock();  // Exclusive for writes
        data_[key] = std::move(value);
    }
};
```

**Performance**: ~30-60ns shared, ~50-100ns exclusive

### SpinlockSynchronizationPolicy

Busy-wait spinlock for short critical sections.

```cpp
class FastCounter : private SpinlockSynchronizationPolicy {
    std::atomic<int> value_{0};
public:
    void increment() {
        auto lock = this->lock();
        // Very short critical section
        ++value_;
    }
};
```

**Performance**: ~5-15ns (uncontended), degrades under contention

**Use when**: Critical sections < 100ns, low contention expected.

---

## Advanced Policies

### RCUPolicy (Read-Copy-Update)

Lock-free reads with copy-on-write updates.

```cpp
template <typename T>
class RCUContainer : private RCUPolicy {
    std::shared_ptr<T> data_;
public:
    // Lock-free read
    std::shared_ptr<const T> read() const {
        return this->rcu_read();  // ~5ns
    }
    
    // Copy-modify-swap update
    template <typename F>
    void update(F&& modifier) {
        this->rcu_update([&](auto& ptr) {
            auto new_data = std::make_shared<T>(*ptr);
            modifier(*new_data);
            ptr = new_data;
        });
    }
};
```

**Performance**: Reads ~5ns, Updates ~100-500ns  
**Use when**: Read-heavy (>90% reads), infrequent updates

### SeqLockPolicy

Optimistic locking for read-mostly data.

```cpp
class Timestamp : private SeqLockPolicy {
    uint64_t seconds_;
    uint32_t nanos_;
public:
    std::pair<uint64_t, uint32_t> read() const {
        return this->seq_read([this]() {
            return std::make_pair(seconds_, nanos_);
        });
        // Retries automatically if writer was active
    }
    
    void write(uint64_t s, uint32_t n) {
        auto lock = this->seq_write_lock();
        seconds_ = s;
        nanos_ = n;
    }
};
```

**Performance**: Reads ~2-10ns, Writes ~20-50ns  
**Use when**: Data fits in cache line, writes rare

### TicketLockPolicy

FIFO-fair spinlock preventing starvation.

```cpp
class FairResource : private TicketLockPolicy {
    Resource resource_;
public:
    void use() {
        auto lock = this->lock();  // Fair ordering
        resource_.do_something();
    }
};
```

**Performance**: ~10-30ns per acquire  
**Use when**: Fairness required, moderate contention

### MCSLockPolicy

Scalable queue-based lock for NUMA systems.

```cpp
class NUMAFriendly : private MCSLockPolicy {
    Data data_;
public:
    void process() {
        auto lock = this->lock();
        // Each waiter spins on local cache line
        data_.process();
    }
};
```

**Performance**: ~15-40ns, scales better than ticket lock  
**Use when**: High contention, NUMA architecture

### HazardPointerPolicy

Safe memory reclamation for lock-free structures.

```cpp
template <typename T>
class LockFreeStack : private HazardPointerPolicy {
    std::atomic<Node*> head_;
public:
    void push(T value) {
        auto* node = new Node{value, head_.load()};
        while (!head_.compare_exchange_weak(node->next, node));
    }
    
    std::optional<T> pop() {
        auto hp = this->acquire_hazard_pointer();
        Node* old_head;
        do {
            old_head = head_.load();
            if (!old_head) return std::nullopt;
            hp.protect(old_head);  // Protect from reclamation
        } while (!head_.compare_exchange_weak(old_head, old_head->next));
        
        T value = old_head->value;
        this->retire(old_head);  // Deferred deletion
        return value;
    }
};
```

**Use when**: Building lock-free data structures

### AdaptiveLockPolicy

Hybrid that spins then sleeps.

```cpp
class AdaptiveContainer : private AdaptiveLockPolicy {
    // Spins for short waits, blocks for long waits
    // Best of both worlds
};
```

**Performance**: Adapts to contention level  
**Use when**: Unpredictable contention patterns

---

## Policy Traits

### Compile-Time Detection

```cpp
// Check policy capabilities
static_assert(is_shared_lock_policy_v<SharedMutexPolicy>);
static_assert(is_recursive_policy_v<RecursiveMutexPolicy>);
static_assert(is_timed_policy_v<TimedMutexPolicy>);
static_assert(is_lock_free_policy_v<RCUPolicy>);

// Contention tracking
if constexpr (has_contention_tracking_v<Policy>) {
    auto stats = policy.get_contention_stats();
}
```

### Available Traits

| Trait | True For |
|-------|----------|
| `is_shared_lock_policy_v` | SharedMutex, SharedTimed |
| `is_recursive_policy_v` | RecursiveMutex |
| `is_timed_policy_v` | TimedMutex, SharedTimed |
| `is_lock_free_policy_v` | RCU, HazardPointer, SeqLock |
| `is_fair_policy_v` | TicketLock, MCS |
| `has_contention_tracking_v` | Mutex, Spinlock, Adaptive |

---

## Usage Patterns

### Pattern 1: Template Parameter

```cpp
template <typename SyncPolicy = MutexSynchronizationPolicy>
class ThreadSafeContainer : private SyncPolicy {
    // ...
};

// Instantiate with desired policy
using STContainer = ThreadSafeContainer<SingleThreadedPolicy>;
using MTContainer = ThreadSafeContainer<MutexSynchronizationPolicy>;
```

### Pattern 2: Policy Injection

```cpp
class Container {
    std::unique_ptr<SyncPolicyBase> sync_;
public:
    explicit Container(std::unique_ptr<SyncPolicyBase> sync)
        : sync_(std::move(sync)) {}
};
```

### Pattern 3: Conditional Compilation

```cpp
#ifdef SINGLE_THREADED
    using DefaultSync = SingleThreadedPolicy;
#else
    using DefaultSync = MutexSynchronizationPolicy;
#endif

template <typename T>
class Container : private DefaultSync { /* ... */ };
```

### Pattern 4: Reader-Writer Separation

```cpp
template <typename T>
class Database : private SharedMutexPolicy {
public:
    T read(int id) const {
        auto lock = this->lock_shared();  // Concurrent reads
        return data_.at(id);
    }
    
    void write(int id, T value) {
        auto lock = this->lock();  // Exclusive write
        data_[id] = std::move(value);
    }
};
```

---

## Performance Guide

### Uncontended Performance (approximate)

| Policy | Lock | Unlock | Notes |
|--------|------|--------|-------|
| SingleThreaded | 0ns | 0ns | Compiled out |
| Mutex | 20ns | 15ns | System call sometimes |
| SharedMutex (shared) | 30ns | 20ns | |
| SharedMutex (exclusive) | 50ns | 30ns | |
| Spinlock | 5ns | 3ns | CPU-bound |
| TicketLock | 10ns | 5ns | Fair |
| MCS | 15ns | 10ns | NUMA-friendly |
| RCU (read) | 5ns | 0ns | Lock-free |
| SeqLock (read) | 2ns | 0ns | May retry |

### Contended Performance

Under high contention:
- **Mutex**: Degrades gracefully (OS scheduling)
- **Spinlock**: Poor (wastes CPU)
- **TicketLock**: Fair but slower
- **MCS**: Scales well
- **RCU**: Readers unaffected

### Selection Guidelines

```
Single-threaded? → SingleThreadedPolicy

Read-heavy (>80% reads)?
├─ Rare updates? → RCUPolicy
├─ Frequent reads of small data? → SeqLockPolicy
└─ General case? → SharedMutexPolicy

Write-heavy or balanced?
├─ Short critical sections (<100ns)? → SpinlockSynchronizationPolicy
├─ Need fairness? → TicketLockPolicy
├─ NUMA system? → MCSLockPolicy
└─ General case? → MutexSynchronizationPolicy

Real-time requirements? → PriorityInheritanceLockPolicy
Need condition variables? → WaitableSynchronizationPolicy
Building lock-free structure? → HazardPointerPolicy
```

---

## Best Practices

### Do

```cpp
// ✅ Use SingleThreadedPolicy for single-threaded code
template <typename Sync = SingleThreadedPolicy>
class Container : private Sync { };

// ✅ Keep critical sections short
{
    auto lock = this->lock();
    value_ = new_value;  // Quick operation
}  // Release immediately

// ✅ Use shared locks for reads
auto lock = this->lock_shared();
return data_;  // Multiple readers OK

// ✅ Consider RCU for read-heavy cases
if constexpr (read_ratio > 0.9) {
    using Sync = RCUPolicy;
}
```

### Don't

```cpp
// ❌ Don't hold locks during I/O
{
    auto lock = this->lock();
    file.write(data_);  // Blocks other threads!
}

// ❌ Don't use spinlocks for long operations
{
    auto lock = spinlock.lock();
    expensive_computation();  // Wastes CPU
}

// ❌ Don't nest locks without RecursiveMutexPolicy
auto lock1 = this->lock();
auto lock2 = this->lock();  // Deadlock!

// ❌ Don't assume lock-free means wait-free
// RCU readers are wait-free, but writers may block
```

---

## C++20/23 Enhancements

### C++20 Features

- `std::atomic<std::shared_ptr>` in RCUPolicy
- `std::jthread` support in WaitableSynchronizationPolicy
- Improved `std::atomic_ref` usage

### C++23 Features

- `std::stop_token` integration
- Enhanced `std::atomic` operations

---

## Related Components

- **AtomicReference.h**: Thread-safe smart pointer
- **LockFreeQueue.h**: Uses HazardPointerPolicy
- **ThreadPool.h**: Uses WaitableSynchronizationPolicy
- **ObjectPool.h**: Configurable synchronization

---

**Document Version:** 1.0  
**Last Updated:** November 2025
